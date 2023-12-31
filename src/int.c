#include "int.h"

#include "cpu.h"
#include "fifo.h"

#include <stdbool.h>

// Interrupt request event representation
typedef struct irq_t {
    // true if IRQ is asserted by the peripheral
    bool request;

    // byte placed on the bus by the peripheral, when doing
    // interrupt handshake with Z80
    uint8_t byte;
} irq_t;

// number of peripherals with pending interrupt requests
static unsigned int pending = 0;
// interrupt requests lines status (sort of)
static irq_t irqs[INTPRIO_COUNT];

void int_irq(int_priority_t priority, uint8_t byte) {
    assert(priority >= 0);
    assert(priority < ARRAY_SIZE(irqs));

    if (!irqs[priority].request)
        ++pending;

    irqs[priority].request = true;
    irqs[priority].byte = byte;
}

static void int_poll(void) {
    if (pending == 0)
        return;

    cpu_int(true);
}

void int_cancel(int_priority_t priority) {
    assert(priority >= 0);
    assert(priority < ARRAY_SIZE(irqs));

    if (irqs[priority].request)
        --pending;

    irqs[priority].request = false;
}

uint8_t int_pop(void) {
    assert(pending);

    // byte to be returned by the peripheral handshake
    uint8_t byte = 0;

    // find interrupt request with maximum priority
    for (size_t i = 0; i < ARRAY_SIZE(irqs); ++i) {
        if (irqs[i].request) {
            byte = irqs[i].byte;
            irqs[i].request = false;
            break;
        }
    }

    // remember one less interrupt request is pending
    --pending;

    // de-assert IRQ line, but only if there are no more pending interrupts
    if (pending == 0)
        cpu_int(false);

    // return device supplied byte
    return byte;
}

void int_init(CEDAModule *mod) {
    // cancel any pending interrupt request
    for (size_t i = 0; i < ARRAY_SIZE(irqs); ++i) {
        irqs[i].request = false;
    }

    // initialize module struct
    mod->init = int_init;
    mod->poll = int_poll;
}
