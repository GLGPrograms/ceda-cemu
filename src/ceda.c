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

static CEDAModule mod_cpu;
static CEDAModule mod_cli;
static CEDAModule mod_gui;
static CEDAModule mod_video;
static CEDAModule mod_speaker;

static CEDAModule *modules[] = {
    &mod_cli, &mod_gui, &mod_cpu, &mod_video, &mod_speaker,
};

void ceda_init(void) {
    cli_init(&mod_cli);
    gui_init(&mod_gui);

    upd8255_init();
    rom_bios_init();
    video_init(&mod_video);
    speaker_init(&mod_speaker);
    bus_init();
    cpu_init(&mod_cpu);
}

void ceda_run(void) {
    // start all modules
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        void (*start)(void) = modules[i]->start;
        if (start)
            start();
    }

    // main loop
    for (;;) {
        // poll all modules
        for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
            void (*poll)(void) = modules[i]->poll;
            if (poll)
                poll();
        }

        // decide wether to exit
        if (gui_isQuit() || cli_isQuit()) {
            break;
        }

        // check for how long each module can sleep, and yield host cpu
        us_time_t wait = LONG_MAX;
        for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
            remaining_handler_t remaining = modules[i]->remaining;
            if (!remaining)
                continue;
            wait = MIN(remaining(), wait);
        }
        if (wait > 0)
            usleep(wait);
    }

    // cleanup all modules
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        void (*cleanup)(void) = modules[i]->cleanup;
        if (cleanup)
            cleanup();
    }
}
