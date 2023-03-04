/***************************************************************************
 *   Copyright (C) 2023 by Daniele Cattaneo                                *
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

//This file contains private implementation details of condition variables
//and semaphores, it's not meant to be used by end-users

#ifndef SYNC_PRIVATE_H
#define SYNC_PRIVATE_H

#include "kernel.h"

namespace miosix_private {

/**
 * \internal
 * Similar to an intrusive list but more lightweight and specifically intended
 * for saving the list of threads waiting on a synchronization object.<br>
 * Currently the list is implemented as a linked-list-backed FIFO. By changing
 * this class it is possible to change the wakeup policy.
 */
struct IntrusiveWaitList
{
    /**
     * Initialize the list.
     */
    IntrusiveWaitList(): first(nullptr), last(nullptr) {}

    /**
     * A node in the list representing a thread. Meant to be stored in the
     * stack of the thread that it represents.
     */
    struct Node
    {
        /**
         * Initialize a new list node and add it to the specified list.
         */
        inline Node(IntrusiveWaitList& parent): next(nullptr)
        {
            this->thread = miosix::Thread::getCurrentThread();
            if (parent.last)
                parent.last->next = this;
            else
                parent.first = this;
            parent.last = this;
        }

        /**
         * \returns true if the thread where this node is stored is still in
         * the wait list.
         */
        inline bool inQueue()
        {
            return this->thread != nullptr;
        }

        ~Node() = default;

        /// The thread waiting, or nullptr if the wait has ended.
        miosix::Thread * volatile thread;
        /// The next thread in the linked list.
        Node *next;

    private:
        // Disallow copies.
        Node(Node const &) = delete;
        void operator=(Node const &x) = delete;
    };

    /**
     * Removes the first thread to wakeup from the list (following FIFO policy)
     * and returns it.
     * \returns a thread to be woken up.
     */
    inline miosix::Thread *pop()
    {
        if (!first)
            return nullptr;
        miosix::Thread *thread = first->thread;
        first->thread = nullptr;
        first = first->next;
        if (!first)
            last = nullptr;
        return thread;
    }

private:
    // Disallow copies.
    IntrusiveWaitList(IntrusiveWaitList const &) = delete;
    void operator=(IntrusiveWaitList const &x) = delete;
    
    Node *first; ///< Pointer to first node of the wait list
    Node *last; ///< Pointer to last node of the wait list
};

}

#endif
