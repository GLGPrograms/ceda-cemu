#include "ceda.h"

#include "assert.h"
#include "bios.h"
#include "bus.h"
#include "cli.h"
#include "cpu.h"
#include "gui.h"
#include "limits.h"
#include "macro.h"
#include "module.h"
#include "speaker.h"
#include "upd8255.h"
#include "video.h"

#include <unistd.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

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

    speaker_start();
    video_start(); // crt emulation

    for (;;) {
        cli_poll();
        gui_poll();

        cpu_poll();
        video_poll();

        if (gui_isQuit() || cli_isQuit()) {
            break;
        }

        static const remaining_handler_t remaining_handlers[] = {
            cli_remaining,
            gui_remaining,
            cpu_remaining,
            video_remaining,
        };

        long wait = LONG_MAX;
        for (unsigned int i = 0; i < ARRAY_SIZE(remaining_handlers); ++i) {
            remaining_handler_t remaining = remaining_handlers[i];
            wait = MIN(remaining(), wait);
        }
        wait = MAX(wait, 0);
        usleep(wait * 1000);
    }

    gui_cleanup();
    cli_cleanup();
}
