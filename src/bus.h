#ifndef CEDA_BUS_H
#define CEDA_BUS_H

#include "type.h"

#include <Z80.h>
#include <stddef.h>

void bus_init(void);

/* memory operations */
zuint8 bus_mem_read(ceda_address_t address);
void bus_mem_readsome(uint8_t *blob, ceda_address_t address, ceda_size_t len);
void bus_mem_write(ceda_address_t address, uint8_t value);

/* I/O operations */
zuint8 bus_io_in(ceda_ioaddr_t address);
void bus_io_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_BUS_H
