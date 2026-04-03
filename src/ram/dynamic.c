#include "dynamic.h"

#include "../units.h"

#include <stdint.h>

#define DYNAMIC_RAM_SIZE (64 * KiB)
static uint8_t ram[DYNAMIC_RAM_SIZE] = {0};

uint8_t dyn_ram_read(uint16_t address) {
    return ram[address];
}

void dyn_ram_write(uint16_t address, uint8_t value) {
    ram[address] = value;
}
