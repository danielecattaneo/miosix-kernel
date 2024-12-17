/***************************************************************************
 *   Copyright (C) 2010-2018 by Terraneo Federico                          *
 *   Copyright (C) 2024 by Daniele Cattaneo                                *
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

#include <cstring>
#include <errno.h>
#include <termios.h>
#include "stm32f7_serial.h"
#include "stm32_serial_common.h"
#include "kernel/sync.h"
#include "kernel/scheduler/scheduler.h"
#include "filesystem/ioctl.h"
#include "cache/cortexMx_cache.h"
#include "interfaces/gpio.h"

using namespace std;

//Work around ST renaming register fields for some STM32L4
#if defined(USART_CR1_RXNEIE_RXFNEIE) && !defined(USART_CR1_RXNEIE)
#define USART_CR1_RXNEIE    USART_CR1_RXNEIE_RXFNEIE
#endif
#if defined(USART_ISR_RXNE_RXFNE) && !defined(USART_ISR_RXNE)
#define USART_ISR_RXNE      USART_ISR_RXNE_RXFNE
#endif
#if defined(USART_ISR_TXE_TXFNF) && !defined(USART_ISR_TXE)
#define USART_ISR_TXE       USART_ISR_TXE_TXFNF
#endif

namespace miosix {

/*
 * Auxiliary class that encapsulates all parts of code that differ between
 * between each instance of the USART peripheral.
 * 
 * Try not to use the attributes of this class directly even if they are public.
 */

class STM32SerialHW
{
public:
    inline USART_TypeDef *get() const { return port; }
    inline IRQn_Type getIRQn() const { return irq; }
    inline unsigned char getAltFunc() const { return altFunc; }
    inline unsigned int IRQgetClock() const { return STM32Bus::getClock(bus); }
    inline void IRQenable() const { STM32Bus::IRQen(bus, clkEnMask); }
    inline void IRQdisable() const { STM32Bus::IRQdis(bus, clkEnMask); }
    inline const STM32SerialDMAHW& getDma() const { return dma; }

    USART_TypeDef *port;        ///< USART port
    IRQn_Type irq;              ///< USART IRQ number
    unsigned char altFunc;      ///< Alternate function to set for GPIOs
    STM32Bus::ID bus;           ///< Bus where the port is (APB1 or 2)
    unsigned long clkEnMask;    ///< USART clock enable

    STM32SerialDMAHW dma;
};

/*
 * Table of hardware configurations
 */

#if defined(STM32F765xx)
constexpr int maxPorts = 8;
static const STM32SerialHW ports[maxPorts] = {
    { USART1, USART1_IRQn, 7, STM32Bus::APB2, RCC_APB2ENR_USART1EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA2EN,
        DMA2_Stream7, DMA2_Stream7_IRQn, STM32SerialDMAHW::Stream7, 4,
        DMA2_Stream5, DMA2_Stream5_IRQn, STM32SerialDMAHW::Stream5, 4 } },
    { USART2, USART2_IRQn, 7, STM32Bus::APB1, RCC_APB1ENR_USART2EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Stream6, DMA1_Stream6_IRQn, STM32SerialDMAHW::Stream6, 4,
        DMA1_Stream5, DMA1_Stream5_IRQn, STM32SerialDMAHW::Stream5, 4 } },
    { USART3, USART3_IRQn, 7, STM32Bus::APB1, RCC_APB1ENR_USART3EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Stream3, DMA1_Stream3_IRQn, STM32SerialDMAHW::Stream3, 4,
        DMA1_Stream1, DMA1_Stream1_IRQn, STM32SerialDMAHW::Stream1, 4 } },
    { UART4 , UART4_IRQn , 8, STM32Bus::APB1, RCC_APB1ENR_UART4EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Stream4, DMA1_Stream4_IRQn, STM32SerialDMAHW::Stream4, 4,
        DMA1_Stream2, DMA1_Stream2_IRQn, STM32SerialDMAHW::Stream2, 4 } },
    { UART5 , UART5_IRQn , 8, STM32Bus::APB1, RCC_APB1ENR_UART5EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Stream7, DMA1_Stream7_IRQn, STM32SerialDMAHW::Stream7, 4,
        DMA1_Stream0, DMA1_Stream0_IRQn, STM32SerialDMAHW::Stream0, 4 } },
    { USART6, USART6_IRQn, 8, STM32Bus::APB2, RCC_APB2ENR_USART6EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA2EN,
        DMA2_Stream6, DMA2_Stream6_IRQn, STM32SerialDMAHW::Stream6, 5,
        DMA2_Stream1, DMA2_Stream1_IRQn, STM32SerialDMAHW::Stream1, 5 } },
    { UART7 , UART7_IRQn , 8, STM32Bus::APB1, RCC_APB1ENR_UART7EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Stream1, DMA1_Stream1_IRQn, STM32SerialDMAHW::Stream1, 5,
        DMA1_Stream3, DMA1_Stream3_IRQn, STM32SerialDMAHW::Stream3, 5 } },
    { UART8 , UART8_IRQn , 8, STM32Bus::APB1, RCC_APB1ENR_UART8EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Stream0, DMA1_Stream0_IRQn, STM32SerialDMAHW::Stream0, 5,
        DMA1_Stream6, DMA1_Stream6_IRQn, STM32SerialDMAHW::Stream6, 5 } },
};
#elif defined(STM32L476xx)
constexpr int maxPorts = 6;
static const STM32SerialHW ports[maxPorts] = {
    { USART1, USART1_IRQn, 7, STM32Bus::APB2, RCC_APB2ENR_USART1EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Channel4, DMA1_Channel4_IRQn, STM32SerialDMAHW::Channel4, 2,
        DMA1_Channel5, DMA1_Channel5_IRQn, STM32SerialDMAHW::Channel5, 2 } },
    { USART2, USART2_IRQn, 7, STM32Bus::APB1L, RCC_APB1ENR1_USART2EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Channel7, DMA1_Channel7_IRQn, STM32SerialDMAHW::Channel7, 2,
        DMA1_Channel6, DMA1_Channel6_IRQn, STM32SerialDMAHW::Channel6, 2 } },
    { USART3, USART3_IRQn, 7, STM32Bus::APB1L, RCC_APB1ENR1_USART3EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Channel2, DMA1_Channel2_IRQn, STM32SerialDMAHW::Channel2, 2,
        DMA1_Channel3, DMA1_Channel3_IRQn, STM32SerialDMAHW::Channel3, 2 } },
    { UART4 , UART4_IRQn , 8, STM32Bus::APB1L, RCC_APB1ENR1_UART4EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA2_Channel5, DMA2_Channel5_IRQn, STM32SerialDMAHW::Channel5, 2,
        DMA2_Channel3, DMA2_Channel3_IRQn, STM32SerialDMAHW::Channel3, 2 } },
    { UART5 , UART5_IRQn , 8, STM32Bus::APB1L, RCC_APB1ENR1_UART5EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA2_Channel1, DMA2_Channel1_IRQn, STM32SerialDMAHW::Channel1, 2,
        DMA2_Channel2, DMA2_Channel2_IRQn, STM32SerialDMAHW::Channel2, 2 } },
    { LPUART1, LPUART1_IRQn, 8, STM32Bus::APB1H, RCC_APB1ENR2_LPUART1EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA2_Channel7, DMA2_Channel7_IRQn, STM32SerialDMAHW::Channel7, 4,
        DMA2_Channel6, DMA2_Channel6_IRQn, STM32SerialDMAHW::Channel6, 4 } },
};
#elif defined(STM32L4R9xx)
constexpr int maxPorts = 6;
static const STM32SerialHW ports[maxPorts] = {
    { USART1, USART1_IRQn, 7, STM32Bus::APB2, RCC_APB2ENR_USART1EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Channel4, DMA1_Channel4_IRQn, STM32SerialDMAHW::Channel4, {4, 25},
        DMA1_Channel5, DMA1_Channel5_IRQn, STM32SerialDMAHW::Channel5, {5, 24} } },
    { USART2, USART2_IRQn, 7, STM32Bus::APB1L, RCC_APB1ENR1_USART2EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Channel7, DMA1_Channel7_IRQn, STM32SerialDMAHW::Channel7, {7, 27},
        DMA1_Channel6, DMA1_Channel6_IRQn, STM32SerialDMAHW::Channel6, {6, 26} } },
    { USART3, USART3_IRQn, 7, STM32Bus::APB1L, RCC_APB1ENR1_USART3EN,
      { DMA1, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA1_Channel2, DMA1_Channel2_IRQn, STM32SerialDMAHW::Channel2, {2, 29},
        DMA1_Channel3, DMA1_Channel3_IRQn, STM32SerialDMAHW::Channel3, {3, 28} } },
    { UART4 , UART4_IRQn , 8, STM32Bus::APB1L, RCC_APB1ENR1_UART4EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA2_Channel5, DMA2_Channel5_IRQn, STM32SerialDMAHW::Channel5, {7+5, 31},
        DMA2_Channel3, DMA2_Channel3_IRQn, STM32SerialDMAHW::Channel3, {7+3, 30} } },
    { UART5 , UART5_IRQn , 8, STM32Bus::APB1L, RCC_APB1ENR1_UART5EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA2_Channel1, DMA2_Channel1_IRQn, STM32SerialDMAHW::Channel1, {7+1, 33},
        DMA2_Channel2, DMA2_Channel2_IRQn, STM32SerialDMAHW::Channel2, {7+2, 32} } },
    { LPUART1, LPUART1_IRQn, 8, STM32Bus::APB1H, RCC_APB1ENR2_LPUART1EN,
      { DMA2, STM32Bus::AHB1, RCC_AHB1ENR_DMA1EN,
        DMA2_Channel7, DMA2_Channel7_IRQn, STM32SerialDMAHW::Channel7, {7+7, 35},
        DMA2_Channel6, DMA2_Channel6_IRQn, STM32SerialDMAHW::Channel6, {7+6, 34} } },
};
#else
#error Unsupported STM32 chip for this serial driver
#endif

//
// class STM32SerialBase
//

// A note on the baudrate/500: the buffer is selected so as to withstand
// 20ms of full data rate. In the 8N1 format one char is made of 10 bits.
// So (baudrate/10)*0.02=baudrate/500
STM32SerialBase::STM32SerialBase(int id, int baudrate, bool flowControl) : 
    flowControl(flowControl), portId(id), rxQueue(rxQueueMin+baudrate/500)
{
    if(id<1 || id>maxPorts) errorHandler(UNEXPECTED);
    port=&ports[id-1];
    if(port->get()==nullptr) errorHandler(UNEXPECTED);
}

void STM32SerialBase::commonInit(int id, int baudrate, GpioPin tx, GpioPin rx,
                    GpioPin rts, GpioPin cts)
{
    //First we set the AF then the mode to avoid glitches
    tx.alternateFunction(port->getAltFunc());
    tx.mode(Mode::ALTERNATE);
    rx.alternateFunction(port->getAltFunc());
    rx.mode(Mode::ALTERNATE);
    if(flowControl)
    {
        rts.alternateFunction(port->getAltFunc());
        rts.mode(Mode::ALTERNATE);
        cts.alternateFunction(port->getAltFunc());
        cts.mode(Mode::ALTERNATE);
    }
    unsigned int freq=port->IRQgetClock();
    unsigned int quot=2*freq/baudrate;      //2*freq for round to nearest
    port->get()->BRR=quot/2 + (quot & 1);   //Round to nearest
    if(flowControl==false) port->get()->CR3 |= USART_CR3_ONEBIT;
    else port->get()->CR3 |= USART_CR3_ONEBIT | USART_CR3_RTSE | USART_CR3_CTSE;
}

void STM32SerialBase::IRQwrite(const char *str)
{
    while(*str)
    {
        while((port->get()->ISR & USART_ISR_TXE)==0) ;
        port->get()->TDR=*str++;
    }
    waitSerialTxFifoEmpty();
}

inline void STM32SerialBase::waitSerialTxFifoEmpty()
{
    while((port->get()->ISR & USART_ISR_TC)==0) ;
}

ssize_t STM32SerialBase::readFromRxQueue(void *buffer, size_t size)
{
    Lock<FastMutex> l(rxMutex);
    char *buf=reinterpret_cast<char*>(buffer);
    size_t result=0;
    FastInterruptDisableLock dLock;
    DeepSleepLock dpLock;
    for(;;)
    {
        //Try to get data from the queue
        for(;result<size;result++)
        {
            if(rxQueue.tryGet(buf[result])==false) break;
            //This is here just not to keep IRQ disabled for the whole loop
            FastInterruptEnableLock eLock(dLock);
        }
        if(idle && result>0) break;
        if(result==size) break;
        //Wait for data in the queue
        do {
            rxWaiting=Thread::IRQgetCurrentThread();
            Thread::IRQenableIrqAndWait(dLock);
        } while(rxWaiting);
    }
    return result;
}

void STM32SerialBase::rxWakeup()
{
    if(rxWaiting)
    {
        rxWaiting->IRQwakeup();
        if(rxWaiting->IRQgetPriority()>
            Thread::IRQgetCurrentThread()->IRQgetPriority())
                IRQinvokeScheduler();
        rxWaiting=nullptr;
    }
}

int STM32SerialBase::ioctl(int cmd, void* arg)
{
    if(reinterpret_cast<unsigned>(arg) & 0b11) return -EFAULT; //Unaligned
    termios *t=reinterpret_cast<termios*>(arg);
    switch(cmd)
    {
        case IOCTL_SYNC:
            waitSerialTxFifoEmpty();
            return 0;
        case IOCTL_TCGETATTR:
            t->c_iflag=IGNBRK | IGNPAR;
            t->c_oflag=0;
            t->c_cflag=CS8 | (flowControl ? CRTSCTS : 0);
            t->c_lflag=0;
            return 0;
        case IOCTL_TCSETATTR_NOW:
        case IOCTL_TCSETATTR_DRAIN:
        case IOCTL_TCSETATTR_FLUSH:
            //Changing things at runtime unsupported, so do nothing, but don't
            //return error as console_device.h implements some attribute changes
            return 0;
        default:
            return -ENOTTY; //Means the operation does not apply to this descriptor
    }
}

//
// class STM32Serial
//

STM32Serial::STM32Serial(int id, int baudrate, GpioPin tx, GpioPin rx)
    : STM32SerialBase(id,baudrate,false), Device(Device::TTY)
{
    commonInit(id,baudrate,tx,rx,tx,rx); //The last two args will be ignored
}

STM32Serial::STM32Serial(int id, int baudrate, GpioPin tx, GpioPin rx,
                         GpioPin rts, GpioPin cts)
    : STM32SerialBase(id,baudrate,true), Device(Device::TTY)
{
    commonInit(id,baudrate,tx,rx,rts,cts);
}

void STM32Serial::commonInit(int id, int baudrate, GpioPin tx, GpioPin rx,
                             GpioPin rts, GpioPin cts)
{
    InterruptDisableLock dLock;
    port->IRQenable();
    IRQregisterIrq(port->getIRQn(),&STM32Serial::IRQhandleInterrupt,this);
    NVIC_SetPriority(port->getIRQn(),15);//Lowest priority for serial
    NVIC_EnableIRQ(port->getIRQn());
    STM32SerialBase::commonInit(id,baudrate,tx,rx,rts,cts);
    //Enabled, 8 data bit, no parity, interrupt on character rx
    port->get()->CR1 = USART_CR1_UE     //Enable port
                     | USART_CR1_RXNEIE //Interrupt on data received
                     | USART_CR1_IDLEIE //Interrupt on idle line
                     | USART_CR1_TE     //Transmission enbled
                     | USART_CR1_RE;    //Reception enabled
}

ssize_t STM32Serial::readBlock(void *buffer, size_t size, off_t where)
{
    return STM32SerialBase::readFromRxQueue(buffer, size);
}

ssize_t STM32Serial::writeBlock(const void *buffer, size_t size, off_t where)
{
    Lock<FastMutex> l(txMutex);
    DeepSleepLock dpLock;
    const char *buf=reinterpret_cast<const char*>(buffer);
    for(size_t i=0;i<size;i++)
    {
        while((port->get()->ISR & USART_ISR_TXE)==0) ;
        port->get()->TDR=*buf++;
    }
    return size;
}

void STM32Serial::IRQhandleInterrupt()
{
    unsigned int status=port->get()->ISR;
    char c;
    rxUpdateIdle(status);
    if(status & USART_ISR_RXNE)
    {
        //Always read data, since this clears interrupt flags
        c=port->get()->RDR;
        //If no error put data in buffer
        if((status & USART_ISR_FE)==0)
            if(rxQueuePut(c)==false) /*fifo overflow*/;
    }
    if(status & USART_ISR_IDLE)
    {
        port->get()->ICR=USART_ICR_IDLECF; //clears interrupt flags
    }
    if((status & USART_ISR_IDLE) || rxQueue.size()>=rxQueueMin)
    {
        //Enough data in buffer or idle line, awake thread
        rxWakeup();
    }
}

void STM32Serial::IRQwrite(const char *str)
{
    // We can reach here also with only kernel paused, so make sure
    // interrupts are disabled
    bool interrupts=areInterruptsEnabled();
    if(interrupts) fastDisableInterrupts();
    STM32SerialBase::IRQwrite(str);
    if(interrupts) fastEnableInterrupts();
}

STM32Serial::~STM32Serial()
{
    waitSerialTxFifoEmpty();
    {
        InterruptDisableLock dLock;
        port->get()->CR1=0;
        NVIC_DisableIRQ(port->getIRQn());
        NVIC_ClearPendingIRQ(port->getIRQn());
        IRQunregisterIrq(port->getIRQn());
        port->IRQdisable();
    }
}

//
// class STM32DMASerial
//

STM32DMASerial::STM32DMASerial(int id, int baudrate, GpioPin tx, GpioPin rx)
    : STM32SerialBase(id,baudrate,false), Device(Device::TTY)
{
    commonInit(id,baudrate,tx,rx,tx,rx); //The last two args will be ignored
}

STM32DMASerial::STM32DMASerial(int id, int baudrate, GpioPin tx, GpioPin rx,
            GpioPin rts, GpioPin cts)
    : STM32SerialBase(id,baudrate,true), Device(Device::TTY)
{
    commonInit(id,baudrate,tx,rx,rts,cts);
}

void STM32DMASerial::commonInit(int id, int baudrate, GpioPin tx, GpioPin rx,
                    GpioPin rts, GpioPin cts)
{
    //Check if DMA is supported for this port
    auto dma=port->getDma();
    if(!dma.get()) errorHandler(UNEXPECTED);
    InterruptDisableLock dLock;

    dma.IRQenable();
    IRQregisterIrq(dma.getTxIRQn(),&STM32DMASerial::IRQhandleDmaTxInterrupt,this);
    NVIC_SetPriority(dma.getTxIRQn(),15);
    NVIC_EnableIRQ(dma.getTxIRQn());
    //Higher priority to ensure IRQhandleDmaRxInterrupt() is called before
    //IRQhandleInterrupt(), so that idle is set correctly
    IRQregisterIrq(dma.getRxIRQn(),&STM32DMASerial::IRQhandleDmaRxInterrupt,this);
    NVIC_SetPriority(dma.getRxIRQn(),14);
    NVIC_EnableIRQ(dma.getRxIRQn());
    dma.IRQinit();

    port->IRQenable();
    IRQregisterIrq(port->getIRQn(),&STM32DMASerial::IRQhandleInterrupt,this);
    NVIC_SetPriority(port->getIRQn(),15);//Lowest priority for serial
    NVIC_EnableIRQ(port->getIRQn());

    STM32SerialBase::commonInit(id,baudrate,tx,rx,rts,cts);

    port->get()->CR3 = USART_CR3_DMAT | USART_CR3_DMAR; //Enable USART DMA
    port->get()->CR1 = USART_CR1_UE     //Enable port
                     | USART_CR1_IDLEIE //Interrupt on idle line
                     | USART_CR1_TE     //Transmission enbled
                     | USART_CR1_RE;    //Reception enabled
    IRQstartDmaRead();
}

ssize_t STM32DMASerial::writeBlock(const void *buffer, size_t size, off_t where)
{
    Lock<FastMutex> l(txMutex);
    DeepSleepLock dpLock;
    const char *buf=reinterpret_cast<const char*>(buffer);
    size_t remaining=size;
    //If the client-provided buffer is not in CCM, we can use it directly
    //as DMA source memory (zero copy).
    if(isInCCMarea(buf)==false) 
    {
        //We don't use zero copy for the last txBufferSize bytes because in this
        //way we can return from this function a bit earlier.
        //If we returned while the DMA was still reading from the client
        //buffer, and the buffer is immediately rewritten, shit happens
        while(remaining>txBufferSize)
        {
            //DMA is limited to 64K
            size_t transferSize=min<size_t>(remaining-txBufferSize,65535);
            waitDmaWriteEnd();
            startDmaWrite(buf,transferSize);
            buf+=transferSize;
            remaining-=transferSize;
        }
    }
    //DMA out all remaining data through txBuffer
    while(remaining>0)
    {
        size_t transferSize=min(remaining,static_cast<size_t>(txBufferSize));
        waitDmaWriteEnd();
        //Copy to txBuffer only after DMA xfer completed, as the previous
        //xfer may be using the same buffer
        memcpy(txBuffer,buf,transferSize);
        startDmaWrite(txBuffer,transferSize);
        buf+=transferSize;
        remaining-=transferSize;
    }
    #ifdef WITH_DEEP_SLEEP
    //The serial driver by default can return even though the last part of
    //the data is still being transmitted by the DMA. When using deep sleep
    //however the DMA operation needs to be fully enclosed by a deep sleep
    //lock to prevent the scheduler from stopping peripheral clocks.
    waitDmaWriteEnd();
    waitSerialTxFifoEmpty(); //TODO: optimize by doing it only when entering deep sleep
    #endif //WITH_DEEP_SLEEP
    return size;
}

void STM32DMASerial::startDmaWrite(const char *buffer, size_t size)
{
    markBufferBeforeDmaWrite(buffer,size);
    DeepSleepLock dpLock;
    //The TC (Transfer Complete) bit in the Status Register (SR) of the serial
    //port can be reset by writing to it directly, or by first reading it
    //out and then writing to the Data Register (DR).
    //  The waitSerialTxFifoEmpty() function relies on the status of TC to
    //accurately reflect whether the serial port is pushing out bytes or not,
    //so it is extremely important for TC to *only* be reset at the beginning of
    //a transmission.
    //  Since the DMA peripheral only writes to DR and never reads from SR,
    //we must read from SR manually first to ensure TC is cleared as soon as
    //the DMA writes to it -- and not earlier!
    //  The alternative is to zero out TC by hand, but if we do that TC becomes
    //unreliable as a flag. Consider the case in which we are clearing out TC
    //in this routine before configuring the DMA. If the DMA fails to start due
    //to an error, or the CPU is reset before the DMA transfer is started, the
    //USART won't be pushing out bytes but TC will still be zero. As a result,
    //waitSerialTxFifoEmpty() will loop forever waiting for the end of a
    //transfer that is not happening.
    while((port->get()->ISR & USART_ISR_TXE)==0) ;
    
    dmaTxInProgress=true;
    //The reinterpret cast is needed because ST, in its infinite wisdom, decided
    //that in L4 headers this register is now 16 bit. Please, nameless engineers
    //at ST, stop fighting on the names and register definitions!
    port->getDma().startDmaWrite(
        reinterpret_cast<volatile uint32_t *>(&port->get()->TDR),buffer,size);
}

void STM32DMASerial::IRQhandleDmaTxInterrupt()
{
    port->getDma().IRQhandleDmaTxInterrupt();
    dmaTxInProgress=false;
    if(txWaiting==nullptr) return;
    txWaiting->IRQwakeup();
    if(txWaiting->IRQgetPriority()>Thread::IRQgetCurrentThread()->IRQgetPriority())
        IRQinvokeScheduler();
    txWaiting=nullptr;
}

void STM32DMASerial::waitDmaWriteEnd()
{
    FastInterruptDisableLock dLock;
    // If a previous DMA xfer is in progress, wait
    if(dmaTxInProgress)
    {
        txWaiting=Thread::IRQgetCurrentThread();
        do {
            Thread::IRQwait();
            {
                FastInterruptEnableLock eLock(dLock);
                Thread::yield();
            }
        } while(txWaiting);
    }
}

ssize_t STM32DMASerial::readBlock(void *buffer, size_t size, off_t where)
{
    return STM32SerialBase::readFromRxQueue(buffer, size);
}

void STM32DMASerial::IRQstartDmaRead()
{
    port->getDma().IRQstartDmaRead(
        reinterpret_cast<volatile uint32_t *>(&port->get()->RDR),
        rxBuffer,rxQueueMin);
}

int STM32DMASerial::IRQstopDmaRead()
{
    return rxQueueMin - port->getDma().IRQstopDmaRead();
}

void STM32DMASerial::IRQflushDmaReadBuffer()
{
    int elem=IRQstopDmaRead();
    markBufferAfterDmaRead(rxBuffer,rxQueueMin);
    for(int i=0;i<elem;i++)
        if(rxQueue.tryPut(rxBuffer[i])==false) /*fifo overflow*/;
    IRQstartDmaRead();
}

void STM32DMASerial::IRQhandleDmaRxInterrupt()
{
    IRQflushDmaReadBuffer();
    rxUpdateIdle(0);
    rxWakeup();
}

void STM32DMASerial::IRQhandleInterrupt()
{
    unsigned int status=port->get()->ISR;
    rxUpdateIdle(status);
    if(status & USART_ISR_IDLE)
    {
        port->get()->ICR=USART_ICR_IDLECF; //clears interrupt flags
        IRQflushDmaReadBuffer();
    }
    if((status & USART_ISR_IDLE) || rxQueue.size()>=rxQueueMin)
    {
        //Enough data in buffer or idle line, awake thread
        rxWakeup();
    }
}

void STM32DMASerial::IRQwrite(const char *str)
{
    //We can reach here also with only kernel paused, so make sure
    //interrupts are disabled
    bool interrupts=areInterruptsEnabled();
    if(interrupts) fastDisableInterrupts();
    //Wait until DMA xfer ends. EN bit is cleared by hardware on transfer end
    port->getDma().IRQwaitDmaWriteStop();
    STM32SerialBase::IRQwrite(str);
    if(interrupts) fastEnableInterrupts();
}

STM32DMASerial::~STM32DMASerial()
{
    waitSerialTxFifoEmpty();
    {
        InterruptDisableLock dLock;
        port->get()->CR1=0;
        IRQstopDmaRead();
        auto dma=port->getDma();
        NVIC_DisableIRQ(dma.getTxIRQn());
        NVIC_ClearPendingIRQ(dma.getTxIRQn());
        IRQunregisterIrq(dma.getTxIRQn());
        NVIC_DisableIRQ(dma.getRxIRQn());
        NVIC_ClearPendingIRQ(dma.getRxIRQn());
        IRQunregisterIrq(dma.getRxIRQn());
        NVIC_DisableIRQ(port->getIRQn());
        NVIC_ClearPendingIRQ(port->getIRQn());
        IRQunregisterIrq(port->getIRQn());
        port->IRQdisable();
    }
}

} //namespace miosix