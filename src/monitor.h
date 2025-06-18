#ifndef CEDA_MONITOR_H
#define CEDA_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum monitor_kind_t {
    MONITOR_EXEC,

    MONITOR_READ_MEM,
    MONITOR_WRITE_MEM,

    MONITOR_READ_IO,
    MONITOR_WRITE_IO,
} monitor_kind_t;

// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
typedef struct Monitor {
    bool valid;
    monitor_kind_t kind;
    uint16_t address;

    // When an exec breakpoint is hit, the instruction is not actually
    // executed: this field is used to skip the breakpoint the first time that
    // is encountered after that the execution has been resumed.
    // For watchpoints (eg. non-exec), this is always false.
    bool skip;

    // If bind_value == true, then the monitor is triggered
    // only if a specific value is written to the address.
    // This is only implemented for memory write and io output operations.
    // If bind_value == false, the the monitor is always triggered,
    // regardless of the target value.
    bool bind_value;

    // If bind_value == true, this contains the monitor-specific value.
    uint8_t value;
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
bool monitor_addBreakpoint(uint16_t address);

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
bool monitor_addReadWatchpoint(uint16_t address);

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
bool monitor_addWriteWatchpoint(uint16_t address, const uint8_t *value);

/**
 * @brief Add a program read watchpoint from the I/O address space.
 *
 * @param address I/O address to watch for input.
 * @return true if the watchpoint has been set, false otherwise.
 */
bool monitor_addInWatchpoint(uint16_t address);

/**
 * @brief Add a program write watchpoint to the I/O address space.
 *
 * @param address I/O address to watch for output.
 * @param value Pointer to the specific value that triggers the watchpoint, or
 * NULL to trigger on any value.
 * @return true if the watchpoint has been set, false otherwise.
 */
bool monitor_addOutWatchpoint(uint16_t address, const uint8_t *value);

/**
 * @brief Remove a program execution monitor.
 *
 * @param index Index of the breakpoint.
 * @return true if the breakpoint has been removed, false otherwise.
 */
bool monitor_delete(unsigned int index);

/**
 * @brief Return a list of the installed breakpoints.
 *
 * This is mainly useful to give a feedback to the user.
 * The whole array of breakpoints is returned, invalid ones too.
 * Check for validity before using.
 *
 * @param vec Pointer to the breakpoints list.
 * @return Total number of breakpoints.
 */
size_t monitor_get(const Monitor *vec[]);

/**
 * @brief Check if a breakpoint has been hit at the given address.
 *
 * Important: calling this function has side effects.
 * Hitting a breakpoint for the first time will tag it to be skipped
 * the next time, otherwise the program won't continue properly,
 * since the CPU will try again to execute the instruction at the breakpoint
 * location.
 *
 * @param address The address to check.
 * @return true if the breakpoint has been hit, false otherwise.
 */
bool monitor_checkBreakpoint(uint16_t address);

/**
 * @brief Check if a watchpoint has been hit while reading at the given address.
 *
 * @param address The address to check.
 * @return true if the read watchpoint has been hit, false otherwise.
 */
bool monitor_checkReadWatchpoint(uint16_t address);

/**
 * @brief Check if a watchpoint has been hit while writing at the given address.
 *
 * The write watchpoint is triggered depending on its configuration.
 * (eg. wether the written value is important or not)
 *
 * @param address The address to check.
 * @param value The value being written.
 * @return true if the write watchpoint has been hit, false otherwise.
 */
bool monitor_checkWriteWatchpoint(uint16_t address, uint8_t value);

/**
 * @brief Check if a watchpoint has been hit while reading from the given I/O
 * address.
 *
 * @param address The address to check (only least significant octet)
 * @return true if the input watchpoint has been hit, false otherwise.
 */
bool monitor_checkInWatchpoint(uint16_t address);

/**
 * @brief Check if a watchpoint has been hit while writing to the given I/O
 * address.
 *
 * @param address The address to check (only least significant octet)
 * @param value The value being written.
 * @return true if the output watchpoint has been hit, false otherwise.
 */
bool monitor_checkOutWatchpoint(uint16_t address, uint8_t value);

/**
 * @brief Return human-readable string description for a monitor kind.
 *
 * @param kind Monitor kind.
 * @return String to describe the monitor kind.
 */
const char *monitor_getKindDescription(monitor_kind_t kind);

#endif // CEDA_MONITOR_H
