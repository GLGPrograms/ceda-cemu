#ifndef CEDA_BUS_H
#define CEDA_BUS_H

#include "module.h"
#include "type.h"

#include <Z80.h>
#include <stdbool.h>
#include <stddef.h>

void bus_init(CEDAModule *mod);

/* memory operations */
zuint8 bus_mem_read(ceda_address_t address);
void bus_mem_readsome(uint8_t *blob, ceda_address_t address, ceda_size_t len);
void bus_mem_write(ceda_address_t address, uint8_t value);

/* I/O operations */
zuint8 bus_io_in(ceda_ioaddr_t address);
void bus_io_out(ceda_ioaddr_t address, uint8_t value);

/* Interrupts */
typedef void (*bus_int_ack_callback_t)(void);

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
void bus_intPush(uint8_t byte, bus_int_ack_callback_t callback);

/**
 * @brief Read the byte provided during the Mode 2 interrupt from the data bus.
 *
 * This function is called by the CPU when it is ready to serve the interrupt.
 * Calling this function also performs the hardware acknowledge of the interrupt
 * with the requesting device.
 *
 * @return uint8_t the device-provided byte
 */
uint8_t bus_intPop(void);

void bus_memSwitch(bool switched);

#endif // CEDA_BUS_H
