#ifndef CEDA_DYNAMIC_RAM_H
#define CEDA_DYNAMIC_RAM_H

#include <Z80.h>

zuint8 dyn_ram_read(zuint16 address);

void dyn_ram_write(zuint16 address, zuint8 value);

#endif // CEDA_DYNAMIC_RAM_H
