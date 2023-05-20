#include "dynamic.h"

#include "../units.h"

#define DYNAMIC_RAM_SIZE (64 * KiB)
static zuint8 ram[DYNAMIC_RAM_SIZE] = {};

zuint8 dyn_ram_read(void *context, zuint16 address) {
    (void)context;

    return ram[address];
}

void dyn_ram_write(void *context, zuint16 address, zuint8 value) {
    (void)context;

    ram[address] = value;
}

