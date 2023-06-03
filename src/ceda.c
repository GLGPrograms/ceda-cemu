#include "ceda.h"

#include "bios.h"
#include "bus.h"
#include "cli.h"
#include "cpu.h"
#include "gui.h"
#include "speaker.h"
#include "upd8255.h"
#include "video.h"

void ceda_init(void) {
    cli_init();
    gui_init();

    upd8255_init();
    rom_bios_init();
    video_init();
    speaker_init();
    bus_init();
    cpu_init();
}

void ceda_run(void) {
    cli_start();
    gui_start();

    speaker_start(); // "beep"
    video_start();   // crt emulation

    for (;;) {
        cli_poll();
        gui_pollEvent();

        cpu_run();
        video_update();

        if (gui_isQuit() || cli_isQuit()) {
            break;
        }
    }

    gui_cleanup();
    cli_cleanup();
}
