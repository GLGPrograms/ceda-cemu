/**
 * \file
 * <!--
 * This file is part of BeRTOS.
 *
 * Bertos is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 *
 * Copyright 2013 Develer S.r.l. (http://www.develer.com/)
 * -->
 *
 * \defgroup fifobuf FIFO buffer
 * \ingroup struct
 * \{
 *
 * \brief General pourpose FIFO buffer implemented with a ring buffer
 *
 * \li \c head points to the element to be extracted next;
 * \li \c tail points to the location following the last insertion;
 * \li when any of the pointers advances beyond \c end, it is reset
 *     back to \c begin.
 *
 * \code
 *
 *  +-----------------------------------+
 *  |  empty  |   valid data   |  empty |
 *  +-----------------------------------+
 *  ^         ^                ^        ^
 *  begin    head             tail     end
 *
 * \endcode
 *
 * The buffer is EMPTY when \c head and \c tail point to the same location:
 *		\code head == tail \endcode
 *
 * The buffer is FULL when \c tail points to the location immediately
 * after \c head:
 *		\code tail == head - 1 \endcode
 *
 * The buffer is also FULL when \c tail points to the last buffer
 * location and head points to the first one:
 *		\code head == begin && tail == end \endcode
 *
 * \author Francesco Sacchi <batt@develer.com>
 */

#ifndef STRUCT_FIFO_H
#define STRUCT_FIFO_H

// #include <cpu/types.h>
// #include <cpu/irq.h>
// #include <cfg/debug.h>

#define DECLARE_FIFO_TYPE(type, fifo_type_name, size)                          \
    typedef struct fifo_type_name {                                            \
        type *volatile head;                                                   \
        type *volatile tail;                                                   \
        type buffer[size];                                                     \
    } fifo_type_name

#define FIFO_END(fifo) &(fifo)->buffer[ARRAY_SIZE((fifo)->buffer) - 1]

/**
 * Check whether the fifo is empty
 *
 * \note Calling fifo_isempty() is safe while a concurrent
 *       execution context is calling fifo_push() or fifo_pop()
 *       only if the CPU can atomically update a pointer.
 *
 */
#define FIFO_ISEMPTY(fb) ((fb)->head == (fb)->tail)

/**
 * Check whether the fifo is full
 *
 * \note Calling fifo_isfull() is safe while a concurrent
 *       execution context is calling fifo_pop() and the
 *       CPU can update a pointer atomically.
 *       It is NOT safe when the other context calls
 *       fifo_push().
 *       This limitation is not usually problematic in a
 *       consumer/producer scenario because the
 *       fifo_isfull() and fifo_push() are usually called
 *       in the producer context.
 */
#define FIFO_ISFULL(fb)                                                        \
    ((((fb)->head == (fb)->buffer) && ((fb)->tail == FIFO_END(fb))) ||         \
     ((fb)->tail == (fb)->head - 1))

/**
 * Push a character on the fifo buffer.
 *
 * \note Calling \c fifo_push() on a full buffer is undefined.
 *       The caller must make sure the buffer has at least
 *       one free slot before calling this function.
 *
 * \note It is safe to call fifo_pop() and fifo_push() from
 *       concurrent contexts, unless the CPU can't update
 *       a pointer atomically (which the AVR and other 8-bit
 *       processors can't do).
 *
 * \sa fifo_push_locked
 */
#define FIFO_PUSH(fb, c)                                                       \
    do {                                                                       \
        *((fb)->tail) = (c);                                                   \
        if ((fb)->tail == FIFO_END(fb))                                        \
            (fb)->tail = (fb)->buffer;                                         \
        else                                                                   \
            (fb)->tail++;                                                      \
    } while (0)

/**
 * Pop a character from the fifo buffer.
 *
 * \note Calling \c fifo_pop() on an empty buffer is undefined.
 *       The caller must make sure the buffer contains at least
 *       one character before calling this function.
 *
 * \note It is safe to call fifo_pop() and fifo_push() from
 *       concurrent contexts.
 */
#define FIFO_POP(fb)                                                           \
    ({                                                                         \
        typeof(*(fb)->head) ret;                                               \
        if ((fb)->head == FIFO_END(fb)) {                                      \
            (fb)->head = (fb)->buffer;                                         \
            ret = *(FIFO_END(fb));                                             \
        } else                                                                 \
            ret = *((fb)->head++);                                             \
        ret;                                                                   \
    })

/**
 * Peek a character from the fifo buffer, without removing it.
 *
 * \note Calling \c fifo_peek() on an empty buffer is undefined.
 *       The caller must make sure the buffer contains at least
 *       one character before calling this function.
 */
#define FIFO_PEEK(fb)                                                          \
    ({                                                                         \
        typeof(*(fb)->head) ret;                                               \
        ret = *((fb)->head);                                                   \
        ret;                                                                   \
    })

/**
 * Make the fifo empty, discarding all its current contents.
 */
#define FIFO_FLUSH(fb) ((fb)->head = (fb)->tail)

/**
 * FIFO Initialization.
 */
#define FIFO_INIT(fb)                                                          \
    do {                                                                       \
        (fb)->head = (fb)->tail = (fb)->buffer;                                \
    } while (0)

/**
 * \return Lenght of the FIFOBuffer \a fb.
 */
#define FIFO_LEN(fb) (FIFO_END(fb) - (fb)->buffer)

#endif /* STRUCT_FIFO_H */
