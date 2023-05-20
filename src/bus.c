#include "bus.h"

#include "macro.h"
#include "ram/dynamic.h"
#include "rom/bios.h"

#include <Z80.h>
#include <inttypes.h>
#include <stdint.h>

#include "log.h"

struct bus_mem_slot {
    uint32_t base;
    uint32_t top;
    Z80Read read;
    Z80Write write;
};

struct bus_mem_slot bus_mem_slots[] = {
    {0xB000, 0xC000, NULL, NULL}, // alt_ram_read/write - TODO
    {0xC000, 0xD000, rom_bios_read, NULL},
    {0xD000, 0xE000, NULL, NULL}, // video_ram_read/write - TODO
};

void bus_init(void) {
    // when starting, BIOS ROM is mounted at 0x0,
    // until the first I/O access is performed,
    // but we'll just emulate this behaviour with an equivalent
    // jmp $c030
    dyn_ram_write(NULL, 0x0, 0xc3);
    dyn_ram_write(NULL, 0x1, 0x30);
    dyn_ram_write(NULL, 0x2, 0xc0);
}

zuint8 bus_read(void *context, zuint16 address) {
    for (size_t i = 0; i < ARRAY_SIZE(bus_mem_slots); ++i) {
        const struct bus_mem_slot *slot = &bus_mem_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->read) {
                const zuint8 value = slot->read(context, address - slot->base);
                LOG_DEBUG("R [%04x] => %02x\n", address, value);
                return value;
            }
        }
    }

    // default: read from dynamic ram
    const zuint8 value = dyn_ram_read(context, address);
    LOG_DEBUG("R [%04x] => %02x\n", address, value);
    return value;
}

void bus_readsome(void *context, void *_blob, zuint16 address, size_t len) {
    zuint8 *blob = (zuint8 *)_blob;

    for (size_t i = 0; i < len; ++i) {
        blob[i] = bus_read(context, address + i);
    }
}

void bus_write(void *context, zuint16 address, zuint8 value) {
    LOG_DEBUG("W [%04x] <= %02x\n", address, value);

    for (size_t i = 0; i < ARRAY_SIZE(bus_mem_slots); ++i) {
        const struct bus_mem_slot *slot = &bus_mem_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->write)
                return slot->write(context, address - slot->base, value);
        }
    }

    // default: write to dynamic ram
    dyn_ram_write(context, address, value);
}
