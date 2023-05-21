#include "ceda.h"

#include "bus.h"
#include "cpu.h"
#include "gui.h"
#include "rom/bios.h"
#include "video.h"

void ceda_init(void) {
    // cli_init();
    gui_init();

    rom_bios_init();
    video_init();
    bus_init();
    cpu_init();
}

void ceda_run(void) {
    gui_start();
    video_start(); // crt emulation

    for (;;) {
        // cli_update();

        cpu_run();
        video_update();
        gui_pollEvent();

        if (gui_isQuit()) {
            break;
        }
    }

    gui_cleanup();
}
