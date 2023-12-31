#ifndef CEDA_INTERRUPT_H
#define CEDA_INTERRUPT_H

#include "module.h"

#include <stdint.h>

/* Interrupts */
typedef void (*int_ack_callback_t)(void);

/**
 * @brief Initialize the interrupt module.
 *
 * @param mod Pointer to CEDAModule struct.
 */
void int_init(CEDAModule *mod);

/**
 * @brief Queue a new interrupt request for Z80 CPU (mode 2).
 *
 * This function can be called by an I/O device when it wants to
 * assert the IRQ line.
 * The device must provide its Mode 2 byte for the CPU, and
 * can provide an (optional) acknowledge callback.
 *
 * @param byte byte to provide to the CPU during the Mode 2 handshake.
 * @param callback callback for hardware interrupt acknowledge
 */
void int_push(uint8_t byte, int_ack_callback_t callback);

/**
 * @brief Read the byte provided during the Mode 2 interrupt from the data bus.
 *
 * This function is called by the CPU when it is ready to serve the interrupt.
 * Calling this function also performs the hardware acknowledge of the interrupt
 * with the requesting device.
 *
 * @return uint8_t the device-provided byte
 */
uint8_t int_pop(void);

#endif // CEDA_INTERRUPT_H
