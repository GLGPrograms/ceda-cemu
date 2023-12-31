#include "int.h"

#include "cpu.h"
#include "fifo.h"

#include <stdbool.h>

// Interrupt event representation
typedef struct int_event_t {
    int_ack_callback_t callback;
    uint8_t byte;
} int_event_t;

// Note: a FIFO is not the suitable struct to handle interrupt requests,
// because we have to handle priority, but:
// - I don't know the Z80 and its peripherals enough
// - we already have a nice FIFO implementation
// - we don't have a priority queue implementation
// So, let's just make this work first, then we will fix.
DECLARE_FIFO_TYPE(int_event_t, int_events_fifo_t, 8);
static int_events_fifo_t int_events_fifo;

void int_push(uint8_t byte, int_ack_callback_t callback) {
    if (FIFO_ISFULL(&int_events_fifo))
        return;

    const int_event_t event = {
        .byte = byte,
        .callback = callback,
    };

    FIFO_PUSH(&int_events_fifo, event);
}

static void int_poll(void) {
    if (FIFO_ISEMPTY(&int_events_fifo))
        return;

    cpu_int(true);
}

uint8_t int_pop(void) {
    assert(!FIFO_ISEMPTY(&int_events_fifo));

    const int_event_t event = FIFO_POP(&int_events_fifo);

    // acknowledge interrupt with I/O device provided callback (if any)
    if (event.callback)
        event.callback();

    // de-assert IRQ line, but only if there are no more pending interrupts
    if (FIFO_ISEMPTY(&int_events_fifo))
        cpu_int(false);

    // return device supplied byte
    return event.byte;
}

void int_init(CEDAModule *mod) {
    // prepare interrupt queue
    FIFO_INIT(&int_events_fifo);

    mod->init = int_init;
    mod->poll = int_poll;
}
