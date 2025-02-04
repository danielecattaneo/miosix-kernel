/***************************************************************************
 *   Copyright (C) 2023-2025 by Terraneo Federico                          *
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

#include "e20/unmember.h"
#include "config/miosix_settings.h"

/**
 * \addtogroup Interfaces
 * \{
 */

/**
 * \file interrupts.h
 * This file provides a common interface to register interrupts in the Miosix
 * kernel. Contrary to Miosix 2.x that used the architecture-specific interrupt
 * registration mechanism, from Miosix 3.0 interrupts are registered at run-time
 * by calling IRQregisterIrq().
 * Additionally, interrupts can be registered with an optional void* argument,
 * or a non-static class member function can be registered as an interrupt.
 *
 * This interface is (currently) only concerned with registering the pointers
 * to the interrupt handler functions, not to setting other properties of
 * interrupts such as their priority, which if needed is still done with
 * architecture-specific code.
 *
 * The interface in this header is meant to be used by all device drivers and
 * code that deals with interrupt handlers.
 *
 * For people who need to implement this interface on a new CPU or architecture,
 * there is one additional function to implement:
 * \code
 * namespace miosix {
 * void IRQinitIrqTable() noexcept;
 * }
 * \endcode
 * that is called during the boot phase to set un the interrupt table, whose
 * implementation shall be to initialize all peripheral interrupt handlers to
 * a default handler so that unexpected interrupts do not cause undefined
 * behavior.
 */

namespace miosix {

/**
 * @name Dynamic Interrupt Handler Registration
 * @{
 */

/**
 * Register an interrupt handler.
 * \param id platform-dependent id of the peripheral for which the handler has
 * to be registered.
 * \param handler pointer to the handler function of type void (*)(void*)
 * \param arg optional void* argument. This argument is stored in the interrupt
 * handling logic and passed as-is whenever the interrupt handler is called.
 * If omitted, the handler function is called with nullptr as argument.
 *
 * \note This function calls errorHandler() causing a reboot if attempting to
 * register an already registered interrupt. If your driver can tolerate failing
 * to register an interrupt you should call IRQisIrqRegistered() to test whether
 * an interrupt is already registered for that id before calling IRQregisterIrq()
 */
void IRQregisterIrq(unsigned int id, void (*handler)(void*), void *arg=nullptr) noexcept;

/**
 * Register an interrupt handler.
 * \param id platform-dependent id of the peripheral for which the handler has
 * to be registered.
 * \param handler pointer to the handler function of type void (*)()
 *
 * \note This function calls errorHandler() causing a reboot if attempting to
 * register an already registered interrupt. If your driver can tolerate failing
 * to register an interrupt you should call IRQisIrqRegistered() to test whether
 * an interrupt is already registered for that id before calling IRQregisterIrq()
 */
inline void IRQregisterIrq(unsigned int id, void (*handler)()) noexcept
{
    IRQregisterIrq(id,reinterpret_cast<void (*)(void*)>(handler));
}

/**
 * Register a class member function as an interrupt handler.
 * \param id platform-dependent id of the peripheral for which the handler has
 * to be registered.
 * \param mfn member function pointer to the class method to be registered as
 * interrupt handler. The method shall take no paprameters.
 * \param object class intance whose methos shall be called as interrupt hanlder.
 *
 * \note This function calls errorHandler() causing a reboot if attempting to
 * register an already registered interrupt. If your driver can tolerate failing
 * to register an interrupt you should call IRQisIrqRegistered() to test whether
 * an interrupt is already registered for that id before calling IRQregisterIrq()
 */
template<typename T>
inline void IRQregisterIrq(unsigned int id, void (T::*mfn)(), T *object) noexcept
{
    auto result=unmember(mfn,object);
    IRQregisterIrq(id,std::get<0>(result),std::get<1>(result));
}

/**
 * Unregister an interrupt handler.
 * \param id platform-dependent id of the peripheral for which the handler has
 * to be unregistered.
 * \param handler pointer to the handler function of type void (*)(void*)
 * \param arg optional void* argument. This argument is stored in the interrupt
 * handling logic and passed as-is whenever the interrupt handler is called.
 * If omitted, the handler function is called with nullptr as argument.
 *
 * \note This function calls errorHandler() causing a reboot if attempting to
 * unregister a different interrupt than the currently registered one
 */
void IRQunregisterIrq(unsigned int id, void (*handler)(void*), void *arg=nullptr) noexcept;

/**
 * Unregister an interrupt handler.
 * \param id platform-dependent id of the peripheral for which the handler has
 * to be registered.
 * \param handler pointer to the handler function of type void (*)()
 *
 * \note This function calls errorHandler() causing a reboot if attempting to
 * unregister a different interrupt than the currently registered one
 */
inline void IRQunregisterIrq(unsigned int id, void (*handler)()) noexcept
{
    IRQunregisterIrq(id,reinterpret_cast<void (*)(void*)>(handler));
}

/**
 * Unregister an interrupt handler.
 * \param id platform-dependent id of the peripheral for which the handler has
 * to be registered.
 * \param mfn member function pointer to the class method to be registered as
 * interrupt handler. The method shall take no paprameters.
 * \param object class intance whose methos shall be called as interrupt hanlder.
 *
 * \note This function calls errorHandler() causing a reboot if attempting to
 * unregister a different interrupt than the currently registered one
 */
template<typename T>
inline void IRQunregisterIrq(unsigned int id, void (T::*mfn)(), T *object) noexcept
{
    auto result=unmember(mfn,object);
    IRQunregisterIrq(id,std::get<0>(result),std::get<1>(result));
}

/**
 * Check whether an interrupt handler is currently registered.
 * \param id platform-dependent id of the peripheral for which to check whether
 * an interrupt handler is registered.
 * \return true if an interrupt hander is currently registered for that id.
 */
bool IRQisIrqRegistered(unsigned int id) noexcept;

/**
 * @}
 */

/**
 * @name Primitives for Interrupt Handlers
 * @{
 */

/**
 * This function is used to develop interrupt driven peripheral drivers.<br>
 * This function can be called from within an interrupt or with interrupts
 * disabled to invoke the scheduler. The request is not performed immediately,
 * it is performed as soon as the interrupt returns or the interrupts are
 * enabled again.
 *
 * As a special exception despite the name, the function is also safe to be
 * called with interrupts enabled, even though you should call Thread::yield()
 * in this case. This function is however NOT safe to be called when the kernel
 * is paused as it will lead to an unwanted context switch and likely a deadlock.
 */
void IRQinvokeScheduler() noexcept;

/**
 * This function serves the double purpose of being a user-available function
 * to disable interrupts with the lowest possible overhead, and as the building
 * block upon which all kernel code and APIs that disable interrupts are built
 * upon, such as \code disableInterrupts() \endcode.
 *
 * Despite being the fastest option, it has a couple of preconditions:
 * - calls to fastDisableInterrupts() can't be nested unlike with
 *   disableInterrupts(). That is, you can't call fastDisableInterrupts()
 *   multiple times and then expect interrupts to be enabled back only ather the
 *   same number of times fastEnableInterrupts() is called.
 * - it can't be used in code that is called before the kernel is started
 */
inline void fastDisableInterrupts() noexcept;

/**
 * This function serves the double purpose of being a user-available function
 * to enable back interruptsthat have been disabled after a call to
 * \code fastDisableInterrupts() \endcode, and as the building
 * block upon which all kernel code and APIs that enable interrupts are built
 * upon, such as \code enableInterrupts() \endcode.
 *
 * Fast version of enableInterrupts().<br>
 * Despite faster, it has a couple of preconditions:
 * - calls to fastEnableInterrupts() can't be nested
 * - it can't be used in code that is called before the kernel is started,
 * because it will (incorrectly) lead to interrupts being enabled before the
 * kernel is started.
 *
 * NOTE: if you disable interrupts with fastDisableInterrupts() then you
 * must enable them back with fastEnableInterrupts(), while if you disable
 * interrupts with disableInterrupts() then you must enable them back with
 * enableInterrupts().
 */
inline void fastEnableInterrupts() noexcept;

/**
 * Provides a way to know if interrupts are enabled or not.
 *
 * \return true if interrupts are enabled
 * \warning Using this function is discouraged
 */
bool areInterruptsEnabled() noexcept;

/**
 * Acquires the global interrupt lock -- a critical section in mutual exclusion
 * with all interrupt handlers -- from an interrupt handler or with interrupts
 * already disabled.
 * 
 * On platforms with support for SMP, an interrupt handler is not in mutual
 * exclusion with code which runs on the other cores (including other interrupt
 * handlers running in parallel). The global interrupt lock is a spinlock used
 * to ensure this mutual exclusion.
 * 
 * On single-core platforms, this function has no effect.
 * 
 * \warning This function can be used only in an interrupt handler or with
 * interrupts disabled.
 */
inline void IRQglobalInterruptLock() noexcept;
#ifndef WITH_SMP
inline void IRQglobalInterruptLock() noexcept {}
#endif

/**
 * Releases the global interrupt lock -- a critical section in mutual exclusion
 * with all interrupt handlers -- from an interrupt handler or with interrupts
 * already disabled.
 * 
 * On platforms with support for SMP, an interrupt handler is not in mutual
 * exclusion with code which runs on the other cores (including other interrupt
 * handlers running in parallel). The global interrupt lock is a spinlock used
 * to ensure this mutual exclusion.
 * 
 * On single-core platforms, this function has no effect.
 * 
 * \warning This function can be used only in an interrupt handler or with
 * interrupts disabled.
 */
inline void IRQglobalInterruptUnlock() noexcept;
#ifndef WITH_SMP
inline void IRQglobalInterruptUnlock() noexcept {}
#endif

/**
 * Acquires the global interrupt lock -- a critical section in mutual exclusion
 * with all interrupt handlers -- from a thread context.
 * 
 * On platforms with support for SMP, an interrupt handler is not in mutual
 * exclusion with code which runs on the other cores (including other interrupt
 * handlers running in parallel). The global interrupt lock is a spinlock used
 * to ensure this mutual exclusion.
 * 
 * On single-core platforms, this function simply disables interrupts.
 */
inline void globalInterruptDisableLock() noexcept
{
    fastDisableInterrupts();
    IRQglobalInterruptLock();
}

/**
 * Releases the global interrupt lock -- a critical section in mutual exclusion
 * with all interrupt handlers -- from a thread context.
 * 
 * On platforms with support for SMP, an interrupt handler is not in mutual
 * exclusion with code which runs on the other cores (including other interrupt
 * handlers running in parallel). The global interrupt lock is a spinlock used
 * to ensure this mutual exclusion.
 * 
 * On single-core platforms, this function simply enables interrupts.
 */
inline void globalInterruptEnableUnlock() noexcept
{
    IRQglobalInterruptUnlock();
    fastEnableInterrupts();
}

/**
 * @}
 */

} //namespace miosix

/**
 * \}
 */

#include "interfaces-impl/interrupts_impl.h"

#ifdef WITH_SMP
#include "interfaces-impl/interrupts_smp_impl.h"
#endif
