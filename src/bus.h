#ifndef CEDA_BUS_H
#define CEDA_BUS_H

#include <Z80.h>
#include <stddef.h>

void bus_init(void);
zuint8 bus_read(void *context, zuint16 address);
void bus_readsome(void *context, void *blob, zuint16 address, size_t len);
void bus_write(void *context, zuint16 address, zuint8 value);

#endif // CEDA_BUS_H
