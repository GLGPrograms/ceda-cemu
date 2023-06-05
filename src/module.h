#ifndef CEDA_MODULE_H
#define CEDA_MODULE_H

#include "time.h"

typedef us_interval_t (*remaining_handler_t)(void);
typedef void (*performance_handler_t)(float *value, const char **unit);

typedef struct CEDAModule {
    /**
     * @brief Initialize module basics.
     *
     * This routine initializes static structures, and prepares basic memory
     * representation of the module.
     * All modules must be initialized, but starting a module could be optional.
     * Example: init video module, but run emulator headless (not implemented)
     *
     * This routine is exported in the public module interface via its .h
     * header, and is called directly by the user in order to populate this
     * struct. Thus, this struct field is special and redundant, and its sole
     * purpose is to document the public module interface organically.
     *
     */
    void (*init)(struct CEDAModule *mod);

    /**
     * @brief Acquire dynamic resources for the module.
     *
     * This routine acquires dynamic resources for the module.
     *
     */
    void (*start)(void);

    /**
     * @brief Advance the internal status of the module.
     *
     * Module poll routine is called periodically by the CEDA main loop.
     * This routine must implement the logic to advance the module status,
     * eg. to execute some instructions, or to update the display.
     *
     * A poll routine usually also updates a counter to remember the last time
     * that has been called, in order to avoid re-running code when it is not
     * actually needed, and possibly compute when to yield the host system CPU.
     *
     */
    void (*poll)(void);

    /**
     * @brief Return the remaining time before next update. [us]
     *
     * A remaining_handler_t must only do the minimal amount of computation
     * needed to return the value of interest. Computation time must be
     * negligible because the main CEDA emulator loop will call the handlers of
     * all the modules in order to determine for how long to yield the CPU to
     * the operating system.
     *
     * Usually, the remaming_handler_t just computes the time difference between
     * the next expected update for the module (based on when poll has been
     * called last time) and the current time.
     *
     */
    remaining_handler_t remaining;

    /**
     * @brief Return the module performance.
     *
     * float* Pointer to float for the metric value.
     * const char** Pointer to null-terminated string representing the
     * measurement unit.
     *
     */
    performance_handler_t performance;

    /**
     * @brief Release module dynamic resources to shut it down.
     *
     * After that the cleanup routine has been called, the module can not be
     * used anymore, and the emulator can be shut down safely.
     */
    void (*cleanup)(void);

} CEDAModule;

#endif // CEDA_MODULE_H
