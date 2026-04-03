#include "auxram.h"

#include "../units.h"

#include <stdint.h>

#define AUXRAM_SIZE (2 * KiB)
static uint8_t ram[AUXRAM_SIZE] = {0};

uint8_t auxram_read(uint16_t address) {
    return ram[address % AUXRAM_SIZE];
}

void auxram_write(uint16_t address, uint8_t value) {
    ram[address % AUXRAM_SIZE] = value;
}
