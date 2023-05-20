#ifndef CEDA_BUS_H
#define CEDA_BUS_H

#include <Z80.h>
#include <stddef.h>

void bus_init(void);

/* memory operations */
zuint8 bus_mem_read(void *context, zuint16 address);
void bus_mem_readsome(void *context, void *blob, zuint16 address, size_t len);
void bus_mem_write(void *context, zuint16 address, zuint8 value);

/* I/O operations */
zuint8 bus_io_in(void *context, zuint16 address);
void bus_io_out(void *context, zuint16 address, zuint8 value);

#endif // CEDA_BUS_H
