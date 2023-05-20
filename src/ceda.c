#include "ceda.h"

#include "cpu.h"
#include "bus.h"

void ceda_init(void) {
    bus_init();
    cpu_init();
}

void ceda_run(void) {
    for (;;) {
        cpu_run();
    }
}

