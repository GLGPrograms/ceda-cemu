#include "bios.h"

#include "conf.h"
#include "module.h"
#include "type.h"
#include "units.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

#define ROM_BIOS_PATH CEDA_PREFIX "/share/ceda/v1.01_rom.bin"
#define ROM_BIOS_SIZE (ceda_size_t)(4 * KiB)

static uint8_t bios[ROM_BIOS_SIZE] = {0};

static bool rom_bios_start(void) {
    bool ret = false;
    const char *rom_path = ROM_BIOS_PATH;
    const char *rom_path_cfg = conf_getString("path", "bios_rom");

    if (rom_path_cfg != NULL)
        rom_path = rom_path_cfg;

    LOG_INFO("Loading BIOS rom from %s\n", rom_path);

    FILE *fp = fopen(rom_path, "rb");

    if (fp == NULL) {
        LOG_ERR("missing bios rom file\n");
        ret = false;
        goto err_missingFile;
    }

    const size_t read = fread(bios, 1, ROM_BIOS_SIZE, fp);
    if (read != ROM_BIOS_SIZE) {
        LOG_ERR("bad bios rom file size: %lu\n", read);
        ret = false;
        goto err_cannotRead;
    }

    ret = true;

err_cannotRead:
    if (fclose(fp) != 0)
        LOG_WARN("error closing bios rom file\n");
err_missingFile:
    return ret;
}

void rom_bios_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = rom_bios_init;
    mod->start = rom_bios_start;
}

uint8_t rom_bios_read(ceda_address_t address) {
    const uint8_t value = bios[address];
    LOG_DEBUG("ROM [%04x] => %02x\n", address, value);
    return value;
}
