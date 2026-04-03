#include "timer.h"

#include <stdint.h>
#include <string.h>

#include "module.h"
#include "type.h"

uint8_t timer_in(ceda_ioaddr_t address) {
    // TODO(giomba)
    (void)address;

    return 0;
}

void timer_out(ceda_ioaddr_t address, uint8_t value) {
    // TODO(giomba)
    (void)address;
    (void)value;
}

static bool timer_restart(void) {
    // TODO(giomba): implement me
    return true;
}

void timer_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = timer_init;
    mod->restart = timer_restart;
    // TODO(giomba)
}
