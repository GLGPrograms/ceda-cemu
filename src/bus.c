#include "bus.h"

#include "crtc.h"
#include "fdc.h"
#include "macro.h"
#include "ram/dynamic.h"
#include "rom/bios.h"
#include "sio2.h"
#include "speaker.h"
#include "timer.h"
#include "upd8255.h"
#include "video.h"

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
    {0xD000, 0xE000, video_ram_read, video_ram_write},
};

struct bus_io_slot {
    uint32_t base;
    uint32_t top;
    Z80Read in;
    Z80Write out;
};

struct bus_io_slot bus_io_slots[] = {
    {0x80, 0x84, upd8255_in, upd8255_out},
    {0xA0, 0xA2, crtc_in, crtc_out},
    {0xB0, 0xB4, sio2_in, sio2_out},
    {0xC0, 0xC2, fdc_in, fdc_out},
    {0xD6, 0xD6, NULL, NULL}, // unknown
    {0xDA, 0xDB, speaker_in, speaker_out},
    {0xDC, 0xDC, NULL, NULL}, // unknown
    {0xDE, 0xDE, NULL, NULL}, // unknown
    {0xE0, 0xE4, timer_in, timer_out},
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

zuint8 bus_mem_read(void *context, zuint16 address) {
    for (size_t i = 0; i < ARRAY_SIZE(bus_mem_slots); ++i) {
        const struct bus_mem_slot *slot = &bus_mem_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->read) {
                const zuint8 value = slot->read(context, address - slot->base);
                LOG_DEBUG("%s: [%04x] => %02x\n", __func__, address, value);
                return value;
            }
        }
    }

    // default: read from dynamic ram
    const zuint8 value = dyn_ram_read(context, address);
    LOG_DEBUG("%s: [%04x] => %02x\n", __func__, address, value);
    return value;
}

void bus_mem_readsome(void *context, void *_blob, zuint16 address, size_t len) {
    zuint8 *blob = (zuint8 *)_blob;

    LOG_DEBUG("%s: [%04x] x %lu\n", __func__, address, len);

    for (size_t i = 0; i < len; ++i) {
        blob[i] = bus_mem_read(context, address + i);
    }
}

void bus_mem_write(void *context, zuint16 address, zuint8 value) {
    LOG_DEBUG("%s: [%04x] <= %02x\n", __func__, address, value);

    for (size_t i = 0; i < ARRAY_SIZE(bus_mem_slots); ++i) {
        const struct bus_mem_slot *slot = &bus_mem_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->write) {
                slot->write(context, address - slot->base, value);
                return;
            }
        }
    }

    // default: write to dynamic ram
    dyn_ram_write(context, address, value);
}

zuint8 bus_io_in(void *context, zuint16 address) {
    LOG_DEBUG("%s: [%02x]\n", __func__, (zuint8)address);

    for (size_t i = 0; i < ARRAY_SIZE(bus_io_slots); ++i) {
        const struct bus_io_slot *slot = &bus_io_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->in)
                return slot->in(context, address - slot->base);
        }
    }

    return 0;
}

void bus_io_out(void *context, zuint16 _address, zuint8 value) {
    const zuint8 address = (zuint8)_address;
    LOG_DEBUG("%s: [%02x] <= %02x\n", __func__, address, value);

    for (size_t i = 0; i < ARRAY_SIZE(bus_io_slots); ++i) {
        const struct bus_io_slot *slot = &bus_io_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->out) {
                slot->out(context, address - slot->base, value);
                return;
            }
        }
    }
}