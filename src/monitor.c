#include "monitor.h"

#include "macro.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

#define BREAKPOINT_CNT 8
static Monitor monitors[BREAKPOINT_CNT] = {0};
static unsigned int valid_breakpoints = 0;

static bool monitor_add(monitor_kind_t kind, uint16_t address,
                        const uint8_t *value) {
    assert((kind == MONITOR_EXEC) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_MEM) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_IO) ? (value == NULL) : true);

    // find free breakpoint slot (if any) and add it
    for (size_t i = 0; i < BREAKPOINT_CNT; ++i) {
        if (!monitors[i].valid) {
            monitors[i].valid = true;
            monitors[i].kind = kind;
            monitors[i].address = address;
            monitors[i].skip = false;

            monitors[i].bind_value = (value != NULL);
            monitors[i].value = value ? *value : 0x55;

            ++valid_breakpoints;
            return true;
        }
    }
    return false;
}

bool monitor_addBreakpoint(uint16_t address) {
    return monitor_add(MONITOR_EXEC, address, NULL);
}

bool monitor_addReadWatchpoint(uint16_t address) {
    return monitor_add(MONITOR_READ_MEM, address, NULL);
}

bool monitor_addWriteWatchpoint(uint16_t address, const uint8_t *value) {
    return monitor_add(MONITOR_WRITE_MEM, address, value);
}

bool monitor_addInWatchpoint(uint16_t address) {
    return monitor_add(MONITOR_READ_IO, address, NULL);
}

bool monitor_addOutWatchpoint(uint16_t address, const uint8_t *value) {
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

static bool monitor_check(monitor_kind_t kind, uint16_t address,
                          const uint8_t *value) {
    assert((kind == MONITOR_EXEC) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_MEM) ? (value == NULL) : true);
    assert((kind == MONITOR_READ_IO) ? (value == NULL) : true);

    if (!valid_breakpoints)
        return false;

    // strip high octet from I/O operations address
    if (kind == MONITOR_READ_IO || kind == MONITOR_WRITE_IO)
        address &= 0xff;

    for (size_t i = 0; i < BREAKPOINT_CNT; ++i) {
        Monitor *monitor = &monitors[i];
        if (!monitor->valid)
            continue;

        if (monitor->kind != kind)
            continue;

        if (monitor->address != address)
            continue;

        if (value && monitor->bind_value && monitor->value != *value)
            continue;

        // here a valid monitor has been found

        // when an exec breakpoint is hit, the instruction is not actually
        // executed, so when execution is resumed, the breakpoint must be
        // skipped
        if (kind == MONITOR_EXEC) {
            if (monitor->skip) {
                LOG_DEBUG("monitor: skip\n");
                monitor->skip = false;
                return false;
            }

            monitor->skip = true;
        }

        LOG_INFO("monitor: hit %04x (kind = %s)\n", address,
                 monitor_getKindDescription(kind));
        return true;
    }

    return false;
}

bool monitor_checkBreakpoint(uint16_t address) {
    return monitor_check(MONITOR_EXEC, address, NULL);
}

bool monitor_checkReadWatchpoint(uint16_t address) {
    return monitor_check(MONITOR_READ_MEM, address, NULL);
}

bool monitor_checkWriteWatchpoint(uint16_t address, uint8_t value) {
    return monitor_check(MONITOR_WRITE_MEM, address, &value);
}

bool monitor_checkInWatchpoint(uint16_t address) {
    return monitor_check(MONITOR_READ_IO, address, NULL);
}

bool monitor_checkOutWatchpoint(uint16_t address, uint8_t value) {
    return monitor_check(MONITOR_WRITE_IO, address, &value);
}

const char *monitor_getKindDescription(monitor_kind_t kind) {
    static const struct associator_t {
        monitor_kind_t kind;
        const char *description;
    } associators[] = {
        {MONITOR_EXEC, "exec"},       //
        {MONITOR_READ_MEM, "read"},   //
        {MONITOR_WRITE_MEM, "write"}, //
        {MONITOR_READ_IO, "in"},      //
        {MONITOR_WRITE_IO, "out"},    //
    };

    for (size_t i = 0; i < ARRAY_SIZE(associators); ++i) {
        const struct associator_t *associator = &associators[i];
        if (associator->kind == kind)
            return associator->description;
    }

    return "unknown";
}

size_t monitor_get(const Monitor *vector[]) {
    *vector = monitors;
    return BREAKPOINT_CNT;
}
