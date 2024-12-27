#include "auxram.h"

#include "../units.h"

#define AUXRAM_SIZE (2 * KiB)
static zuint8 ram[AUXRAM_SIZE] = {0};

zuint8 auxram_read(zuint16 address) {
    return ram[address % AUXRAM_SIZE];
}

void auxram_write(zuint16 address, zuint8 value) {
    ram[address % AUXRAM_SIZE] = value;
}