#ifndef CEDA_DYNAMIC_RAM_H
#define CEDA_DYNAMIC_RAM_H

#include <Z80.h>

zuint8 dyn_ram_read(void *context, zuint16 address);

void dyn_ram_write(void *context, zuint16 address, zuint8 value);


#endif // CEDA_DYNAMIC_RAM_H

