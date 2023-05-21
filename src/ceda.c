#include "ceda.h"

#include "bus.h"
#include "cpu.h"
#include "rom/bios.h"
#include "video.h"

void ceda_init(void) {
    // cli_init();

    rom_bios_init();
    video_init();
    bus_init();
    cpu_init();
}

void ceda_run(void) {
    for (;;) {
        // cli_update();

        cpu_run();
        video_update();
    }
}
