#include "bus.h"

#include "bios.h"
#include "crtc.h"
#include "fdc.h"
#include "macro.h"
#include "ram/dynamic.h"
#include "sio2.h"
#include "speaker.h"
#include "timer.h"
#include "upd8255.h"
#include "video.h"

#include <Z80.h>
#include <inttypes.h>
#include <stdint.h>

#include "log.h"

typedef uint8_t (*bus_mem_read_t)(ceda_address_t address);
typedef void (*bus_mem_write_t)(ceda_address_t address, uint8_t value);

struct bus_mem_slot {
    ceda_address_t base;
    uint32_t top;
    bus_mem_read_t read;
    bus_mem_write_t write;
};

static const struct bus_mem_slot bus_mem_slots[] = {
    {0xB000, 0xC000, NULL, NULL}, // alt_ram_read/write - TODO
    {0xC000, 0xD000, rom_bios_read, NULL},
    {0xD000, 0xE000, video_ram_read, video_ram_write},
};

typedef uint8_t (*bus_io_read_t)(ceda_ioaddr_t address);
typedef void (*bus_io_write_t)(ceda_ioaddr_t address, uint8_t value);
struct bus_io_slot {
    ceda_ioaddr_t base;
    uint32_t top;
    bus_io_read_t in;
    bus_io_write_t out;
};

static const struct bus_io_slot bus_io_slots[] = {
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
    static const uint8_t jmp[] = {0xc3, 0x30, 0xc0};
    for (uint8_t address = 0; address < (uint8_t)ARRAY_SIZE(jmp); ++address) {
        dyn_ram_write(address, jmp[address]);
    }
}

uint8_t bus_mem_read(ceda_address_t address) {
    for (size_t i = 0; i < ARRAY_SIZE(bus_mem_slots); ++i) {
        const struct bus_mem_slot *slot = &bus_mem_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->read) {
                const zuint8 value = slot->read(address - slot->base);
                LOG_DEBUG("%s: [%04x] => %02x\n", __func__, address, value);
                return value;
            }
        }
    }

    // default: read from dynamic ram
    const zuint8 value = dyn_ram_read(address);
    LOG_DEBUG("%s: [%04x] => %02x\n", __func__, address, value);
    return value;
}

void bus_mem_readsome(uint8_t *blob, ceda_address_t address, ceda_size_t len) {
    LOG_DEBUG("%s: [%04x] x %lu\n", __func__, address, len);

    for (zuint16 i = 0; i < len; ++i) {
        blob[i] = bus_mem_read(address + i);
    }
}

void bus_mem_write(ceda_address_t address, uint8_t value) {
    LOG_DEBUG("%s: [%04x] <= %02x\n", __func__, address, value);

    for (size_t i = 0; i < ARRAY_SIZE(bus_mem_slots); ++i) {
        const struct bus_mem_slot *slot = &bus_mem_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->write) {
                slot->write(address - slot->base, value);
                return;
            }
        }
    }

    // default: write to dynamic ram
    dyn_ram_write(address, value);
}

uint8_t bus_io_in(ceda_ioaddr_t address) {
    LOG_DEBUG("%s: [%02x]\n", __func__, (zuint8)address);

    for (size_t i = 0; i < ARRAY_SIZE(bus_io_slots); ++i) {
        const struct bus_io_slot *slot = &bus_io_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->in) {
                return slot->in(address - slot->base);
            }
        }
    }

    return 0;
}

void bus_io_out(ceda_ioaddr_t _address, uint8_t value) {
    const zuint8 address = (zuint8)_address;
    LOG_DEBUG("%s: [%02x] <= %02x\n", __func__, address, value);

    for (size_t i = 0; i < ARRAY_SIZE(bus_io_slots); ++i) {
        const struct bus_io_slot *slot = &bus_io_slots[i];
        if (address >= slot->base && address < slot->top) {
            if (slot->out) {
                slot->out(address - slot->base, value);
                return;
            }
        }
    }
}