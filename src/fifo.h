/**
 * General purpose FIFO buffer implemented with a ring buffer
 *
 * - head points to the element to be extracted next
 * - tail points to the location following the last insertion
 * - when any of the pointers advances beyond end, it is reset
 *     back to begin
 *
 *
 *  +-----------------------------------+
 *  |  empty  |   valid data   |  empty |
 *  +-----------------------------------+
 *  ^         ^                ^        ^
 *  begin    head             tail     end
 *
 *
 * The buffer is EMPTY when head and tail point to the same location:
 * head == tail
 *
 * The buffer is FULL when tail points to the location immediately
 * after head:
 * tail == head - 1
 *
 * The buffer is also FULL when tail points to the last buffer
 * location and head points to the first one:
 * head == begin && tail == end
 *
 */
#ifndef CEDA_FIFO_H
#define CEDA_FIFO_H

#include "macro.h"

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
 * Calling FIFO_ISEMPTY() is safe while a concurrent
 * execution context is calling FIFO_PUSH() or FIFO_POP()
 * only if the CPU can atomically update a pointer.
 *
 */
#define FIFO_ISEMPTY(fb) ((fb)->head == (fb)->tail)

/**
 * Check whether the fifo is full
 *
 * Calling FIFO_ISFULL() is safe while a concurrent
 * execution context is calling FIFO_POP() and the
 * CPU can update a pointer atomically.
 * It is NOT safe when the other context calls
 * FIFO_PUSH().
 * This limitation is not usually problematic in a
 * consumer/producer scenario because the
 * FIFO_ISFULL() and FIFO_PUSH() are usually called
 * in the producer context.
 */
#define FIFO_ISFULL(fb)                                                        \
    ((((fb)->head == (fb)->buffer) && ((fb)->tail == FIFO_END(fb))) ||         \
     ((fb)->tail == (fb)->head - 1))

/**
 * Push a character on the fifo buffer.
 *
 * Calling FIFO_PUSH() on a full buffer is undefined.
 * The caller must make sure the buffer has at least
 * one free slot before calling this function.
 *
 * It is safe to call FIFO_POP() and FIFO_PUSH() from
 * concurrent contexts, unless the CPU can't update
 * a pointer atomically.
 *
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
 * Calling FIFO_POP() on an empty buffer is undefined.
 * The caller must make sure the buffer contains at least
 * one character before calling this function.
 *
 * It is safe to call FIFO_POP() and FIFO_PUSH() from
 * concurrent contexts.
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
 * Calling FIFO_PEEK() on an empty buffer is undefined.
 * The caller must make sure the buffer contains at least
 * one character before calling this function.
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
 * Return lenght of the FIFO fb.
 */
#define FIFO_LEN(fb) (FIFO_END(fb) - (fb)->buffer)

/**
 * Return number of used elements in the FIFO.
 */
#define FIFO_COUNT(fb)                                                         \
    ({                                                                         \
        long cnt = (fb)->tail - (fb)->head;                                    \
        if (cnt < 0)                                                           \
            cnt += (long)ARRAY_SIZE((fb)->buffer);                             \
        cnt;                                                                   \
    })

/**
 * Return number of free (unused) elements in the FIFO.
 */
#define FIFO_FREE(fb)                                                          \
    ({ /* NOLINT */ (((long)ARRAY_SIZE((fb)->buffer) - 1) - FIFO_COUNT(fb)); })

#endif // CEDA_FIFO_H
