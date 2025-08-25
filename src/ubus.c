#include "ubus.h"

#include "conf.h"

#include <stddef.h>
#include <string.h>

#define UBUS_MAX_PERIPHERALS (4)

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

struct ubus_io_slot {
    ceda_ioaddr_t base;
    uint32_t top;
    ubus_io_read_t in;
    ubus_io_write_t out;
};

static struct ubus_io_slot ubus_slots[UBUS_MAX_PERIPHERALS];
static size_t ubus_used = 0;

void ubus_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));

    mod->init = ubus_init;
}

bool ubus_register(ceda_ioaddr_t base, uint32_t top, ubus_io_read_t read,
                   ubus_io_write_t write) {
    if (!read && !write)
        return false;

    if (top > 0x100)
        return false;

    if (top <= base)
        return false;

    if (ubus_used == UBUS_MAX_PERIPHERALS) {
        LOG_WARN("Too many peripherals registered\n");
        return false;
    }

    // check if peripherals are overlapping
    for (size_t i = 0; i < ubus_used; ++i) {
        struct ubus_io_slot *slot = &ubus_slots[i];
        bool ok = (top <= slot->base) || (base >= slot->top);
        if (!ok)
            return false;
    }

    ubus_slots[ubus_used].base = base;
    ubus_slots[ubus_used].top = top;
    ubus_slots[ubus_used].in = read;
    ubus_slots[ubus_used].out = write;
    ubus_used += 1;

    LOG_INFO("Registered peripheral at %02x\n", (unsigned int)base);

    return true;
}

zuint8 ubus_io_in(ceda_ioaddr_t address) {
    for (size_t i = 0; i < ubus_used; ++i) {
        struct ubus_io_slot *slot = &ubus_slots[i];
        if (address >= slot->base && address < slot->top)
            if (slot->in)
                return slot->in(address - slot->base);
    }
    return 0;
}

void ubus_io_out(ceda_ioaddr_t address, uint8_t value) {
    for (size_t i = 0; i < ubus_used; ++i) {
        struct ubus_io_slot *slot = &ubus_slots[i];
        if (address >= slot->base && address < slot->top)
            if (slot->out) {
                slot->out(address - slot->base, value);
                return;
            }
    }
}
