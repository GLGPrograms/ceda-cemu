#ifndef CEDA_INTERRUPT_H
#define CEDA_INTERRUPT_H

#include "module.h"

#include <stdint.h>

/* Interrupts */
typedef void (*int_ack_callback_t)(void);
/* Interrupts priority, from highest to lowest */
typedef enum int_priority_t {
    INTPRIO_CTC,
    INTPRIO_SIO2,
    INTPRIO_EXT,

    INTPRIO_COUNT, //< countof (max enum value)
} int_priority_t;

/**
 * @brief Initialize the interrupt module.
 *
 * @param mod Pointer to CEDAModule struct.
 */
void int_init(CEDAModule *mod);

/**
 * @brief Assert IRQ line.
 *
 * This function sends an interrupt request for Z80 CPU (mode 2).
 * This function can be called by an I/O device when it wants to
 * assert the IRQ line.
 * The device must provide its Mode 2 byte for the CPU.
 * The device must also indicate its interrupt request priority,
 * which depends on the phisical wiring of IEI/IEO line.
 *
 * @param priority interrupt request priority
 * @param byte byte to provide to the CPU during the Mode 2 handshake.
 */
void int_irq(int_priority_t priority, uint8_t byte);

/**
 * @brief Deassert IRQ line.
 *
 * This function cancels an interrupt request for Z80 CPU. *
 * This function can be called by an I/O device when it wants to
 * de-assert the IRQ line.
 * The device must indicate its interrupt request priority,
 * which depends on the phisical wiring of the IEI/IEO line.
 *
 * @param priority interrupt request priority
 */
void int_cancel(int_priority_t priority);

/**
 * @brief Read the byte provided during the Mode 2 interrupt from the data bus.
 *
 * This function is called by the CPU when it is ready
 * to serve interrupt requests.
 *
 * @return the device-provided byte
 */
uint8_t int_pop(void);

#endif // CEDA_INTERRUPT_H
