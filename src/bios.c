#include "bios.h"

#include "units.h"

#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#define ROM_BIOS_PATH "rom/V1.01_ROM.bin"
#define ROM_BIOS_SIZE (ceda_size_t)(4 * KiB)

static zuint8 bios[ROM_BIOS_SIZE] = {0};

void rom_bios_init(void) {
    FILE *fp = fopen(ROM_BIOS_PATH, "rb");
    if (fp == NULL) {
        LOG_ERR("missing bios rom file\n");
        abort();
    }

    const size_t read = fread(bios, 1, ROM_BIOS_SIZE, fp);
    if (read != ROM_BIOS_SIZE) {
        LOG_ERR("bad bios rom file size: %lu\n", read);
        abort();
    }

    if (fclose(fp) != 0) {
        LOG_ERR("error closing bios rom file\n");
        abort();
    }
}

uint8_t rom_bios_read(ceda_address_t address) {
    const zuint8 value = bios[address];
    LOG_DEBUG("ROM [%04x] => %02x\n", address, value);
    return value;
}
