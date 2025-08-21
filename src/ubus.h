#ifndef CEDA_USER_BUS_H
#define CEDA_USER_BUS_H

#include "module.h"
#include "type.h"

#include <stdbool.h>

#include <Z80.h>

typedef uint8_t (*ubus_io_read_t)(ceda_ioaddr_t address);
typedef void (*ubus_io_write_t)(ceda_ioaddr_t address, uint8_t value);

/**
 * @brief Initialize the User Bus module.
 *
 * This module allows users to connect custom peripherals to the computer bus.
 */
void ubus_init(CEDAModule *mod);

/**
 * @brief Register a peripheral on the bus.
 *
 * @param base Peripheral base address.
 * @param top Peripheral top address + 1 (eg. the first unused address)
 * @param read IO input callback.
 * @param write IO output callback.
 *
 * @return true in case of success, false otherwise.
 */
bool ubus_register(ceda_ioaddr_t base, uint32_t top, ubus_io_read_t read,
                   ubus_io_write_t write);

zuint8 ubus_io_in(ceda_ioaddr_t address);
void ubus_io_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_USER_BUS_H
