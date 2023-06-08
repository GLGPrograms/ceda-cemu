#ifndef CEDA_ROM_BIOS_H
#define CEDA_ROM_BIOS_H

#include "type.h"

#include <Z80.h>

void rom_bios_init(void);

uint8_t rom_bios_read(ceda_address_t address);

#endif // CEDA_ROM_BIOS_H
