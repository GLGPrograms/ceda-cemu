#include "monitor.h"

#include <assert.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

#define BREAKPOINT_CNT 8
static Monitor monitors[BREAKPOINT_CNT] = {0};
static unsigned int valid_breakpoints = 0;
static bool pass = false;

static bool monitor_add(monitor_kind_t kind, zuint16 address,
                        const zuint8 *value) {
    assert((kind == MONITOR_EXEC) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_MEM) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_IO) ? (value == NULL) : true);

    // find free breakpoint slot (if any) and add it
    for (size_t i = 0; i < BREAKPOINT_CNT; ++i) {
        if (!monitors[i].valid) {
            monitors[i].valid = true;
            monitors[i].kind = kind;
            monitors[i].address = address;

            monitors[i].bind_value = (value != NULL);
            monitors[i].value = value ? *value : 0x55;

            // we can assume that the user wants to stop when a monitor is set,
            // even if `continue` has been given before
            pass = false;

            ++valid_breakpoints;
            return true;
        }
    }
    return false;
}

bool monitor_addBreakpoint(zuint16 address) {
    return monitor_add(MONITOR_EXEC, address, NULL);
}

bool monitor_addReadWatchpoint(zuint16 address) {
    return monitor_add(MONITOR_READ_MEM, address, NULL);
}

bool monitor_addWriteWatchpoint(zuint16 address, const zuint8 *value) {
    return monitor_add(MONITOR_WRITE_MEM, address, value);
}

bool monitor_addInWatchpoint(zuint16 address) {
    return monitor_add(MONITOR_READ_IO, address, NULL);
}

bool monitor_addOutWatchpoint(zuint16 address, const zuint8 *value) {
    return monitor_add(MONITOR_WRITE_IO, address, value);
}

bool monitor_delete(unsigned int index) {
    if (index >= BREAKPOINT_CNT)
        return false;

    if (monitors[index].valid == false)
        return false;

    monitors[index].valid = false;
    --valid_breakpoints;
    return true;
}

static bool monitor_check(monitor_kind_t kind, zuint16 address,
                          const zuint8 *value) {
    assert((kind == MONITOR_EXEC) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_MEM) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_IO) ? (value == NULL) : true);

    if (!valid_breakpoints)
        return false;

    for (size_t i = 0; i < BREAKPOINT_CNT; ++i) {
        const Monitor *monitor = &monitors[i];
        if (!monitor->valid)
            continue;

        if (monitor->kind != kind)
            continue;

        if (monitor->address != address)
            continue;

        if (value && monitor->bind_value && monitor->value != *value)
            continue;

        if (pass) {
            LOG_DEBUG("monitor: skip\n");
            pass = false;
            return false;
        }

        LOG_DEBUG("monitor: hit %04x (kind = %d)\n", address, kind);
        return true;
    }

    return false;
}

bool monitor_checkBreakpoint(zuint16 address) {
    return monitor_check(MONITOR_EXEC, address, NULL);
}

bool monitor_checkReadWatchpoint(zuint16 address) {
    return monitor_check(MONITOR_READ_MEM, address, NULL);
}

bool monitor_checkWriteWatchpoint(zuint16 address, zuint8 value) {
    return monitor_check(MONITOR_WRITE_MEM, address, &value);
}

bool monitor_checkInWatchpoint(zuint16 address) {
    return monitor_check(MONITOR_READ_IO, address, NULL);
}

bool monitor_checkOutWatchpoint(zuint16 address, zuint8 value) {
    return monitor_check(MONITOR_WRITE_IO, address, &value);
}

void monitor_pass(void) {
    // If a `continue` command is given before setting any monitor,
    // and a monitor is set afterwards, then the first encountered monitor is
    // skipped. So here we are making sure that pass == true only if there is at
    // least a configured monitor.
    if (valid_breakpoints > 0)
        pass = true;
}

#if 0


bool breakpoint_checkAddress(zuint16 address, zuint8 *next) {
    for (size_t i = 0; i < BREAKPOINT_CNT; ++i) {
        if (!monitors[i].valid)
            continue;
        if (monitors[i].address != address)
            continue;

        LOG_DEBUG("breakpoint: pass = %d\n", (int)pass);
        if (pass) {
            LOG_DEBUG("breakpoint: skip\n");
            pass = false;
            *next = monitors[i].prev;
            return true;
        }

        LOG_DEBUG("breakpoint: hit %04x\n", (int)address);
        return false;
    }

    LOG_DEBUG("hit a legit trap\n");
#define NOP (0x00)
    *next = NOP;
    return true;
}
#endif

size_t monitor_get(const Monitor *vector[]) {
    *vector = monitors;
    return BREAKPOINT_CNT;
}
