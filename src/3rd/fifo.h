/**
 * \brief General pourpose FIFO buffer implemented with a ring buffer
 *
 * - head points to the element to be extracted next;
 * - tail points to the location following the last insertion;
 * - when any of the pointers advances beyond \c end, it is reset
 *     back to \c begin.
 *
 *
 *  +-----------------------------------+
 *  |  empty  |   valid data   |  empty |
 *  +-----------------------------------+
 *  ^         ^                ^        ^
 *  begin    head             tail     end
 *
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
 */
#ifndef CEDA_FIFO_H
#define CEDA_FIFO_H

#include "../compiler.h"

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

/**
 * \return Number of used elements in the FIFO.
 */
#define FIFO_COUNT(fb)                                                         \
    ({                                                                         \
        int cnt = (fb)->tail - (fb)->head;                                     \
        if (cnt < 0)                                                           \
            cnt += countof((fb)->buffer);                                      \
        cnt;                                                                   \
    })

/**
 * \return Number of free (unused) elements in the FIFO.
 */
#define FIFO_FREE(fb) ((countof((fb)->buffer) - 1) - FIFO_COUNT(fb))

#endif // CEDA_FIFO_H
