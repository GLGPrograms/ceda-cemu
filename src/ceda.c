#include "ceda.h"

#include "bus.h"
#include "cpu.h"
#include "rom/bios.h"

void ceda_init(void) {
    rom_bios_init();
    bus_init();
    cpu_init();
}

void ceda_run(void) {
    for (;;) {
        cpu_run();
    }
}
