/***************************************************************************
 *   Copyright (C) 2025 by Daniele Cattaneo                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include "interfaces_private/smp.h"
#include "interfaces/arch_registers.h"
#include "interfaces/cpu_const.h"
#include "kernel/error.h"

// System mode stack size for core 1
#define CORE1_SYSTEM_STACK_SIZE 0x200
// Stringification macros used for embedding the above macro in assembly code
#define XSTR(a) STR(a)
#define STR(a) #a

namespace miosix {

char core1SystemStack[CORE1_SYSTEM_STACK_SIZE];

static void fifoDrain()
{
    while(sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS)
        (unsigned int)sio_hw->fifo_rd;
}

static bool fifoSend(unsigned long s)
{
    // Send
    while(!(sio_hw->fifo_st & SIO_FIFO_ST_RDY_BITS)) ;
    sio_hw->fifo_wr=s;
    __SEV();
    // Read back and check the value is the same
    while(!(sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS)) ;
    unsigned long r=sio_hw->fifo_rd;
    return s==r;
}

static unsigned long fifoReceive()
{
    // Read the new value
    while(!(sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS)) __WFE();
    unsigned long r=sio_hw->fifo_rd;
    // Send the value back for acknowledgment
    while(!(sio_hw->fifo_st & SIO_FIFO_ST_RDY_BITS)) ;
    sio_hw->fifo_wr=r;
    return r;
}

void IRQinterProcessorInterruptHandler()
{
    IRQGlobalInterruptLock lock;
    unsigned long fifoState=sio_hw->fifo_st;
    if(fifoState & (SIO_FIFO_ST_ROE_BITS|SIO_FIFO_ST_WOF_BITS))
    {
        // interrupt was triggered by a fifo error, clear the error
        // TODO: this should not happen, we should call errorHandler(UNEXPECTED) here
        sio_hw->fifo_st=0;
    }
    if(fifoState & SIO_FIFO_ST_VLD_BITS)
    {
        // When we manage to take the GIL, the other CPU must have already finished
        // sending the data so we can read both pointers without checking
        void (*f)(void *)=reinterpret_cast<void (*)(void *)>(sio_hw->fifo_rd);
        // if(!(sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS)) errorHandler(UNEXPECTED);
        void *arg=reinterpret_cast<void *>(sio_hw->fifo_rd);
        f(arg);
    }
}

__attribute__((naked)) void initCore1()
{
    asm volatile(
        "cpsid i                              \n" //Disable interrupts
        "mrs r0, msp                          \n"
        "msr psp, r0                          \n" //Use current stack as PSP
        "ldr r0, =_ZN6miosix16core1SystemStackE+" XSTR(CORE1_SYSTEM_STACK_SIZE) "\n"
        "msr msp, r0                          \n" //Select correct MSP
        "movs r0, #2                          \n" //Privileged, process stack
        "msr control, r0                      \n" //Activate PSP
        "isb                                  \n" //Required when switching stack
        "bl _ZN6miosix20IRQcontinueInitCore1Ev\n" //Continue core 1 init
    );
}

void IRQcontinueInitCore1()
{
    // Read main function info from FIFO
    void (*f)(void *)=reinterpret_cast<void (*)(void *)>(fifoReceive());
    void *arg=reinterpret_cast<void *>(fifoReceive());
    // Initialize all interrupts to a default priority
    NVIC_SetPriority(SVCall_IRQn,defaultIrqPriority);
    NVIC_SetPriority(PendSV_IRQn,defaultIrqPriority);
    NVIC_SetPriority(SysTick_IRQn,defaultIrqPriority);
    const unsigned int numInterrupts=MIOSIX_NUM_PERIPHERAL_IRQ;
    for(unsigned int i=0;i<numInterrupts;i++)
        NVIC_SetPriority(static_cast<IRQn_Type>(i),defaultIrqPriority);
    {
        // Take the GIL with interrupts already disabled. Once we are done with
        // initialization this will seamlessly re-enable interrupts.
        GlobalInterruptLock lock;
        // Register IPI (FIFO) interrupt handler for core 1
        IRQregisterIrq(SIO_IRQ_PROC1_IRQn,IRQinterProcessorInterruptHandler);
        // Clear fifo status flags and pending interrupt flag to avoid spurious
        // interrupts on core 1 side
        sio_hw->fifo_st=0;
        NVIC_ClearPendingIRQ(SIO_IRQ_PROC1_IRQn);
    }
    f(arg);
}

void IRQinitSMP(void *const stackPtrs[], void (*const mains[])(void *), void *const args[]) noexcept
{
    // Send FIFO commands for the bootrom core idling mechanism
    for(;;)
    {
        fifoDrain();
        __SEV();
        if(!fifoSend(0)) continue;
        fifoDrain();
        __SEV();
        if(!fifoSend(0)) continue;
        if(fifoSend(1)) break;
    }
    unsigned long vtor=SCB->VTOR;
    unsigned long psp=reinterpret_cast<unsigned long>(stackPtrs[0]);
    unsigned long pc=reinterpret_cast<unsigned long>(&initCore1);
    if (!fifoSend(vtor)) errorHandler(UNEXPECTED);
    if (!fifoSend(psp)) errorHandler(UNEXPECTED);
    if (!fifoSend(pc)) errorHandler(UNEXPECTED);
    // Send main function address and args, which will be read by
    // IRQcontinueInitCore1
    unsigned long fp=reinterpret_cast<unsigned long>(mains[0]);
    unsigned long arg=reinterpret_cast<unsigned long>(args[0]);
    if (!fifoSend(fp)) errorHandler(UNEXPECTED);
    if (!fifoSend(arg)) errorHandler(UNEXPECTED);
    // Register IPI (FIFO) interrupt handler for core 0
    IRQregisterIrq(SIO_IRQ_PROC0_IRQn,IRQinterProcessorInterruptHandler);
    // Clear fifo status flags and pending interrupt flag to avoid spurious
    // interrupts on core 0 side
    sio_hw->fifo_st=0;
    NVIC_ClearPendingIRQ(SIO_IRQ_PROC0_IRQn);
}

void IRQcallOnCore(unsigned char core, void (*f)(void *), void *arg) noexcept
{
    if(core==getCurrentCoreId()) { f(arg); return; }
    while(!(sio_hw->fifo_st & SIO_FIFO_ST_RDY_BITS)) ;
    sio_hw->fifo_wr=reinterpret_cast<unsigned long>(f);
    while(!(sio_hw->fifo_st & SIO_FIFO_ST_RDY_BITS)) ;
    sio_hw->fifo_wr=reinterpret_cast<unsigned long>(arg);
}

} // namespace miosix
