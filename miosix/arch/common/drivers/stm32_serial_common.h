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

#pragma once

#include "interfaces/arch_registers.h"

/*
 * Common code for STM32 serial drivers
 */

/*
 * We support multiple revisions of the hardware; the following defines select
 * the hardware variant and additional quirks that need to be taken into
 * account in the code.
 */

#if defined(_ARCH_CORTEXM3_STM32F1)
    #define BUS_HAS_AHB
    #define BUS_HAS_APB12
    #define DMA_STM32F1
#elif defined(_ARCH_CORTEXM3_STM32L1)
    #define BUS_HAS_AHB
    #define BUS_HAS_APB12
    #define DMA_STM32F1
#elif defined(_ARCH_CORTEXM4_STM32L4)
    #define BUS_HAS_AHB12
    #define BUS_HAS_APB1L1H2
    #if defined(DMAMUX1_Channel0)
        #define DMA_HAS_MUX
    #else
        #define DMA_HAS_CSELR
    #endif
    #define DMA_STM32F1
#else
    #define BUS_HAS_AHB12
    #define BUS_HAS_APB12
    #define DMA_STM32F2
#endif

namespace miosix {

/*
 * Helper class for handling enabling/disabling peripherals on a STM32 bus
 */

class STM32Bus
{
public:
    enum ID {
        #if defined(BUS_HAS_APB12)
            APB1, APB2,
        #elif defined(BUS_HAS_APB1L1H2)
            APB1L, APB1H, APB2,
        #else // BUS_HAS_APBx
        #error APB compile time bus setting unspecified
        #endif // BUS_HAS_APBx
        #if defined(BUS_HAS_AHB)
            AHB,
        #elif defined(BUS_HAS_AHB12)
            AHB1, AHB2
        #else // BUS_HAS_AHBx
        #error AHB compile time bus setting unspecified
        #endif // BUS_HAS_AHBx
    };

    static inline void IRQen(STM32Bus::ID bus, unsigned long mask)
    {
        getENR(bus)|=mask;
        RCC_SYNC();
    }
    static inline void IRQdis(STM32Bus::ID bus, unsigned long mask)
    {
        getENR(bus)&=mask;
        RCC_SYNC();
    }
    
    static inline unsigned int getClock(STM32Bus::ID bus)
    {
        unsigned int freq=SystemCoreClock;
        switch(bus)
        {
        #if defined(BUS_HAS_APB1L1H2)
            case STM32Bus::APB1L:
            case STM32Bus::APB1H:
        #else
            case STM32Bus::APB1:
        #endif
                if(RCC->CFGR & RCC_CFGR_PPRE1_2)
                    freq/=1<<(((RCC->CFGR>>RCC_CFGR_PPRE1_Pos) & 0x3)+1);
                break;
            case STM32Bus::APB2:
                if(RCC->CFGR & RCC_CFGR_PPRE2_2)
                    freq/=1<<(((RCC->CFGR>>RCC_CFGR_PPRE2_Pos) & 0x3)+1);
                break;
            default:
                break;
        }
        return freq;
    }

private:
    static volatile unsigned long& getENR(STM32Bus::ID bus)
    {
        switch(bus)
        {
            #if defined(BUS_HAS_APB12)
                case STM32Bus::APB1: return RCC->APB1ENR;
                case STM32Bus::APB2: return RCC->APB2ENR;
            #elif defined(BUS_HAS_APB1L1H2)
                case STM32Bus::APB1L: return RCC->APB1ENR1;
                case STM32Bus::APB1H: return RCC->APB1ENR2;
                case STM32Bus::APB2: return RCC->APB2ENR;
            #endif // BUS_HAS_APBx
            default:
            #if defined(BUS_HAS_AHB)
                case STM32Bus::AHB: return RCC->AHBENR;
            #elif defined(BUS_HAS_AHB12)
                case STM32Bus::AHB1: return RCC->AHB1ENR;
                case STM32Bus::AHB2: return RCC->AHB2ENR;
            #endif // BUS_HAS_AHBx
        }
    }
};

#if defined(DMA_HAS_MUX)

class STM32SerialDMAMUXHW
{
public:
    inline void IRQinit()
    {
        get()->CCR&=~DMAMUX_CxCR_DMAREQ_ID;
        get()->CCR|=static_cast<unsigned long>(id)<<DMAMUX_CxCR_DMAREQ_ID_Pos;
    }

    unsigned char channel;
    unsigned char id;

private:
    inline DMAMUX_Channel_TypeDef *get() const
    {
        static DMAMUX_Channel_TypeDef * const ptrs[]={
            DMAMUX1_Channel0,
            DMAMUX1_Channel1,
            DMAMUX1_Channel2,
            DMAMUX1_Channel3,
            DMAMUX1_Channel4,
            DMAMUX1_Channel5,
            DMAMUX1_Channel6,
            DMAMUX1_Channel7,
            DMAMUX1_Channel8,
            DMAMUX1_Channel9,
            DMAMUX1_Channel10,
            DMAMUX1_Channel11,
            DMAMUX1_Channel12,
            DMAMUX1_Channel13
        };
        return ptrs[channel-1];
    }
};

#endif

/*
 * Auxiliary class that encapsulates all parts of code that differ between
 * between each instance of the DMA interface
 * 
 * Try not to use the attributes of this class directly even if they are public,
 * in the current implementation they are not the best use of ROM space and
 * by using getter/setters we make it easier for ourselves to compress this
 * data structure a bit in the future.
 * TODO: Remove redundant information in the attributes
 */

#if defined(DMA_STM32F1)

class STM32SerialDMAHW
{
public:
    enum IntRegShift
    {
        Channel1=0*4, Channel2=1*4, Channel3=2*4, Channel4=3*4,
        Channel5=4*4, Channel6=5*4, Channel7=6*4
    };

    #ifdef DMA2
        inline DMA_TypeDef *get() const { return dma; }
        inline void IRQenable() const { STM32Bus::IRQen(bus, clkEnMask); }
        inline void IRQdisable() const { STM32Bus::IRQdis(bus, clkEnMask); }
    #else //Save some code size if there is only one DMA device
        inline DMA_TypeDef *get() const { return DMA1; }
        inline void IRQenable() const { STM32Bus::IRQen(STM32Bus::AHB, RCC_AHBENR_DMA1EN); }
        inline void IRQdisable() const { STM32Bus::IRQdis(STM32Bus::AHB, RCC_AHBENR_DMA1EN); }
    #endif

    inline IRQn_Type getTxIRQn() const { return txIrq; }
    inline unsigned long getTxISR() const { return getISR(txIRShift); }
    inline void setTxIFCR(unsigned long v) const { return setIFCR(txIRShift,v); }

    inline IRQn_Type getRxIRQn() const { return rxIrq; }
    inline unsigned long getRxISR() const { return getISR(rxIRShift); }
    inline void setRxIFCR(unsigned long v) const { return setIFCR(rxIRShift,v); }

    inline void IRQinit()
    {
        #if defined(DMA_HAS_MUX)
            STM32Bus::IRQen(STM32Bus::AHB1, RCC_AHB1ENR_DMAMUX1EN);
            txMux.IRQinit();
            rxMux.IRQinit();
        #elif defined(DMA_HAS_CSELR)
            //Work around the fact that ST's headers are extremely yabai
            //(yabai means bad in Chinese)
            static_assert(DMA1_CSELR_BASE==DMA1_BASE+0xA8);
            unsigned volatile long *cselr=
                reinterpret_cast<unsigned volatile long *>(
                    reinterpret_cast<uintptr_t>(get())+0xA8);
            unsigned long mask=(0xFUL<<txIRShift) | (0xFUL<<rxIRShift);
            unsigned long val=(txCsel<<txIRShift) | (rxCsel<<rxIRShift);
            *cselr=(*cselr & ~mask) | val;
        #endif
    }

    inline void startDmaWrite(volatile uint32_t *dr, const char *buffer, size_t size) const
    {
        tx->CPAR=reinterpret_cast<unsigned int>(dr);
        tx->CMAR=reinterpret_cast<unsigned int>(buffer);
        tx->CNDTR=size;
        tx->CCR = DMA_CCR_MINC    //Increment RAM pointer
                | DMA_CCR_DIR     //Memory to peripheral
                | DMA_CCR_TCIE    //Interrupt on completion
                | DMA_CCR_TEIE    //Interrupt on transfer error
                | DMA_CCR_EN;     //Start the DMA
    }

    inline void IRQhandleDmaTxInterrupt() const
    {
        setTxIFCR(DMA_IFCR_CGIF1);
        tx->CCR=0; //Disable DMA
    }

    inline void IRQwaitDmaWriteStop() const
    {
        while((tx->CCR & DMA_CCR_EN) && !(getTxISR() & (DMA_ISR_TCIF1|DMA_ISR_TEIF1))) ;
    }

    inline void IRQstartDmaRead(volatile uint32_t *dr, const char *buffer, unsigned int size) const
    {
        rx->CPAR=reinterpret_cast<unsigned int>(dr);
        rx->CMAR=reinterpret_cast<unsigned int>(buffer);
        rx->CNDTR=size;
        rx->CCR = DMA_CCR_MINC    //Increment RAM pointer
                | 0               //Peripheral to memory
                | DMA_CCR_TEIE    //Interrupt on transfer error
                | DMA_CCR_TCIE    //Interrupt on transfer complete
                | DMA_CCR_EN;     //Start the DMA
    }

    inline int IRQstopDmaRead() const
    {
        rx->CCR=0;
        setRxIFCR(DMA_IFCR_CGIF1);
        return rx->CNDTR;
    }

    #ifdef DMA2
        DMA_TypeDef *dma;           ///< Pointer to the DMA peripheral (DMA1/2)
        STM32Bus::ID bus;           ///< Bus where the DMA port is (AHB1 or 2)
        unsigned long clkEnMask;    ///< DMA clock enable bit
    #endif

    DMA_Channel_TypeDef *tx;    ///< Pointer to DMA TX channel
    IRQn_Type txIrq;            ///< DMA TX stream IRQ number
    unsigned char txIRShift;    ///< Value from DMAIntRegShift for the stream
    #if defined(DMA_HAS_MUX)
        STM32SerialDMAMUXHW txMux;
    #elif defined(DMA_HAS_CSELR)
        unsigned char txCsel;
    #endif

    DMA_Channel_TypeDef *rx;    ///< Pointer to DMA RX channel
    IRQn_Type rxIrq;            ///< DMA RX stream IRQ number
    unsigned char rxIRShift;    ///< Value from DMAIntRegShift for the stream
    #if defined(DMA_HAS_MUX)
        STM32SerialDMAMUXHW rxMux;
    #elif defined(DMA_HAS_CSELR)
        unsigned char rxCsel;
    #endif

private:
    inline unsigned long getISR(unsigned char pos) const
    {
        return (get()->ISR>>pos) & 0b1111;
    }
    inline void setIFCR(unsigned char pos, unsigned long value) const
    {
        get()->IFCR=(value&0b1111) << pos;
    }
};

#elif defined(DMA_STM32F2)

class STM32SerialDMAHW
{
public:
    enum IntRegShift
    {
        Stream0= 0, Stream1= 0+6, Stream2=16, Stream3=16+6,
        Stream4=32, Stream5=32+6, Stream6=48, Stream7=48+6
    };

    inline DMA_TypeDef *get() const { return dma; }
    inline void IRQenable() const { STM32Bus::IRQen(bus, clkEnMask); }
    inline void IRQdisable() const { STM32Bus::IRQdis(bus, clkEnMask); }

    inline IRQn_Type getTxIRQn() const { return txIrq; }
    inline unsigned long getTxISR() const { return getISR(txIRShift); }
    inline void setTxIFCR(unsigned long v) const { return setIFCR(txIRShift,v); }

    inline IRQn_Type getRxIRQn() const { return rxIrq; }
    inline unsigned long getRxISR() const { return getISR(rxIRShift); }
    inline void setRxIFCR(unsigned long v) const { return setIFCR(rxIRShift,v); }

    inline void IRQinit() { }

    inline void startDmaWrite(volatile uint32_t *dr, const char *buffer, size_t size) const
    {
        tx->PAR=reinterpret_cast<unsigned int>(dr);
        tx->M0AR=reinterpret_cast<unsigned int>(buffer);
        tx->NDTR=size;
        //Quirk: not enabling DMA_SxFCR_FEIE because at the beginning of a transfer
        //there is always at least one spurious fifo error due to the fact that the
        //FIFO doesn't begin to fill up until after the DMA request is triggered.
        //  This is just a FIFO underrun error and as such it is resolved
        //automatically by the DMA internal logic and does not stop the transfer,
        //just like for FIFO overruns.
        //  On the other hand, a FIFO error caused by register misconfiguration
        //would prevent the transfer from even starting, but the conditions for
        //FIFO misconfiguration are known in advance and we don't fall in any of
        //those cases.
        //  In other words, underrun, overrun and misconfiguration are the only FIFO
        //error conditions; misconfiguration is impossible, and we don't need to do
        //anything for overruns and underruns, so there is literally no reason to
        //enable FIFO error interrupts in the first place.
        tx->FCR=DMA_SxFCR_DMDIS;//Enable fifo
        tx->CR = (txChannel << DMA_SxCR_CHSEL_Pos) //Select channel
               | DMA_SxCR_MINC    //Increment RAM pointer
               | DMA_SxCR_DIR_0   //Memory to peripheral
               | DMA_SxCR_TCIE    //Interrupt on completion
               | DMA_SxCR_TEIE    //Interrupt on transfer error
               | DMA_SxCR_DMEIE   //Interrupt on direct mode error
               | DMA_SxCR_EN;     //Start the DMA
    }

    inline void IRQhandleDmaTxInterrupt() const
    {
        setTxIFCR(DMA_LIFCR_CTCIF0
                | DMA_LIFCR_CTEIF0
                | DMA_LIFCR_CDMEIF0
                | DMA_LIFCR_CFEIF0);
    }

    inline void IRQwaitDmaWriteStop() const
    {
        while(tx->CR & DMA_SxCR_EN) ;
    }

    inline void IRQstartDmaRead(volatile uint32_t *dr, const char *buffer, unsigned int size) const
    {
        rx->PAR=reinterpret_cast<unsigned int>(dr);
        rx->M0AR=reinterpret_cast<unsigned int>(buffer);
        rx->NDTR=size;
        rx->CR = (rxChannel << DMA_SxCR_CHSEL_Pos) //Select channel
               | DMA_SxCR_MINC    //Increment RAM pointer
               | 0                //Peripheral to memory
               | DMA_SxCR_HTIE    //Interrupt on half transfer
               | DMA_SxCR_TEIE    //Interrupt on transfer error
               | DMA_SxCR_DMEIE   //Interrupt on direct mode error
               | DMA_SxCR_EN;     //Start the DMA
    }

    inline int IRQstopDmaRead() const
    {
        //Stop DMA and wait for it to actually stop
        rx->CR &= ~DMA_SxCR_EN;
        while(rx->CR & DMA_SxCR_EN) ;
        setRxIFCR(DMA_LIFCR_CTCIF0
            | DMA_LIFCR_CHTIF0
            | DMA_LIFCR_CTEIF0
            | DMA_LIFCR_CDMEIF0
            | DMA_LIFCR_CFEIF0);
        return rx->NDTR;
    }

    DMA_TypeDef *dma;           ///< Pointer to the DMA peripheral (DMA1/2)
    STM32Bus::ID bus;           ///< Bus where the DMA port is (AHB1 or 2)
    unsigned long clkEnMask;    ///< DMA clock enable bit

    DMA_Stream_TypeDef *tx;     ///< Pointer to DMA TX stream
    IRQn_Type txIrq;            ///< DMA TX stream IRQ number
    unsigned char txIRShift;    ///< Value from DMAIntRegShift for the stream
    unsigned char txChannel;    ///< DMA TX stream channel

    DMA_Stream_TypeDef *rx;     ///< Pointer to DMA RX stream
    IRQn_Type rxIrq;            ///< DMA RX stream IRQ number
    unsigned char rxIRShift;    ///< Value from DMAIntRegShift for the stream
    unsigned char rxChannel;    ///< DMA TX stream channel

private:
    inline unsigned long getISR(unsigned char pos) const
    {
        if(pos<32) return (dma->LISR>>pos) & 0b111111;
        return (dma->HISR>>(pos-32)) & 0b111111;
    }
    inline void setIFCR(unsigned char pos, unsigned long value) const
    {
        value=(value&0b111111) << (pos%32);
        if(pos<32) dma->LIFCR=value;
        else dma->HIFCR=value;
    }
};

#endif // DMA_STM32Fx

} // namespace miosix