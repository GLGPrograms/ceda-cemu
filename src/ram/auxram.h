#ifndef CEDA_ALT_RAM_H
#define CEDA_ALT_RAM_H

#include <stdint.h>

uint8_t auxram_read(uint16_t address);
void auxram_write(uint16_t address, uint8_t value);

#endif // CEDA_ALT_RAM_H
