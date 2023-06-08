#include "dynamic.h"

#include "../units.h"

#define DYNAMIC_RAM_SIZE (64 * KiB)
static zuint8 ram[DYNAMIC_RAM_SIZE] = {0};

zuint8 dyn_ram_read(zuint16 address) {
    return ram[address];
}

void dyn_ram_write(zuint16 address, zuint8 value) {
    ram[address] = value;
}
