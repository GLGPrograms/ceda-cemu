#ifndef CEDA_DYNAMIC_RAM_H
#define CEDA_DYNAMIC_RAM_H

#include <stdint.h>

uint8_t dyn_ram_read(uint16_t address);

void dyn_ram_write(uint16_t address, uint8_t value);

#endif // CEDA_DYNAMIC_RAM_H
