#include "bios.h"

#include "conf.h"
#include "units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define ROM_BIOS_PATH "rom/V1.01_ROM.bin"
#define ROM_BIOS_SIZE (ceda_size_t)(4 * KiB)

static zuint8 bios[ROM_BIOS_SIZE] = {0};

static bool rom_bios_start(void) {
    const char *rom_path = ROM_BIOS_PATH;
    const char *rom_path_cfg = conf_getString("path", "bios_rom");

    if (rom_path_cfg != NULL)
        rom_path = rom_path_cfg;

    LOG_INFO("Loading BIOS rom from %s\n", rom_path);

    FILE *fp = fopen(rom_path, "rb");

    if (fp == NULL) {
        LOG_ERR("missing bios rom file\n");
        return false;
    }

    const size_t read = fread(bios, 1, ROM_BIOS_SIZE, fp);
    if (read != ROM_BIOS_SIZE) {
        LOG_ERR("bad bios rom file size: %lu\n", read);
        return false;
    }

    if (fclose(fp) != 0) {
        LOG_ERR("error closing bios rom file\n");
        return false;
    }

    return true;
}

void rom_bios_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = rom_bios_init;
    mod->start = rom_bios_start;
}

uint8_t rom_bios_read(ceda_address_t address) {
    const zuint8 value = bios[address];
    LOG_DEBUG("ROM [%04x] => %02x\n", address, value);
    return value;
}
