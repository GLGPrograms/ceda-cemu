#ifndef CEDA_ROM_BIOS_H
#define CEDA_ROM_BIOS_H

#include <Z80.h>

void rom_bios_init(void);

zuint8 rom_bios_read(void *context, zuint16 address);

#endif // CEDA_ROM_BIOS_H
