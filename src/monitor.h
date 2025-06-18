#ifndef CEDA_MONITOR_H
#define CEDA_MONITOR_H

#include <Z80.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum monitor_kind_t {
    MONITOR_EXEC,

    MONITOR_READ_MEM,
    MONITOR_WRITE_MEM,

    MONITOR_READ_IO,
    MONITOR_WRITE_IO,
} monitor_kind_t;

typedef struct Monitor {
    bool valid;
    monitor_kind_t kind;
    zuint16 address;

    // Monitor is triggered only on a specific address and value.
    // This can be true only for write/out monitors.
    bool bind_value;

    // If exec monitor: original octet in that memory location,
    // else: the value to monitor for, if bind_value == true
    zuint8 value;
} Monitor;

/**
 * @brief Add a program execution breakpoint.
 *
 * The breakpoint will pause the cpu when the cpu tries to fetch the instruction
 * located at the given address.
 * CPU halt will occur just before the execution of the instruction at the given
 * address.
 *
 * @param address Address of the instruction which must trigger the breakpoint.
 * @return true if the breakpoint has been set, false otherwise.
 */
bool monitor_addBreakpoint(zuint16 address);

/**
 * @brief Add a program read watchpoint.
 *
 * The watchpoint will pause the cpu when the cpu reads a byte from the
 * specified memory address. CPU halt will occur just after the read has been
 * performed.
 *
 * @param address Memory address to watch for reads.
 * @return true if the watchpoint has been set, false otherwise.
 */
bool monitor_addReadWatchpoint(zuint16 address);

/**
 * @brief Add a program write watchpoint.
 *
 * The watchpoint will pause the cpu when the cpu writes a byte at the specified
 * memory address. CPU halt will occur just after the write has been performed.
 * The watchpoint can configured to be triggered for every write access, or only
 * on a specific octet value.
 *
 * @param address Memory address to watch for writes.
 * @param value Pointer to the specific value that triggers the watchpoint, or
 * NULL to trigger on any value.
 * @return true if the watchpoint has been set, false otherwise.
 */
bool monitor_addWriteWatchpoint(zuint16 address, const zuint8 *value);

bool monitor_addInWatchpoint(zuint16 address);

bool monitor_addOutWatchpoint(zuint16 address, const zuint8 *value);

/**
 * @brief Remove a program execution breakpoint.
 *
 * The breakpoint will be removed using the currently active memory bank
 * configuration.
 *
 * @param index Index of the breakpoint.
 * @return true if the breakpoint has been remove, false otherwise.
 */
bool monitor_delete(unsigned int index);

/**
 * @brief Return a list of the installed breakpoints.
 *
 * This is mainly useful to give a feedback to the user.
 * The whole array of breakpoints is returned, invalid ones too.
 * Check for validity before using.
 *
 * @param v Pointer to the breakpoints list.
 * @return Total number of breakpoints.
 */
size_t monitor_get(const Monitor *v[]);

/**
 * @brief Check if a breakpoint has been hit at the given address.
 *
 * @param address The address to check.
 * @return true if a breakpoint has been hit, false otherwise.
 */
bool monitor_checkBreakpoint(zuint16 address);

/**
 * @brief Check if a watchpoint has been hit while reading at the given address.
 *
 * @param address The address to check.
 * @return true if a read watchpoint has been hit, false otherwise.
 */
bool monitor_checkReadWatchpoint(zuint16 address);

/**
 * @brief Check if a watchpoint has been hit while writing at the given address.
 *
 * The write watchpoint is triggered depending on its configuration.
 * (eg. wether the written value is important or not)
 *
 * @param address The address to check.
 * @param value The value being written.
 * @return true if a write watchpoint has been hit, false otherwise.
 */
bool monitor_checkWriteWatchpoint(zuint16 address, zuint8 value);

bool monitor_checkInWatchpoint(zuint16 address);

bool monitor_checkOutWatchpoint(zuint16 address, zuint8 value);

/**
 * @brief Pass/skip the current breakpoint.
 *
 * If no breakpoint has been hit, the next encountered breakpoint will be
 * skipped.
 */
void monitor_pass(void);

#endif // CEDA_MONITOR_H
