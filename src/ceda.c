#include "ceda.h"

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

#include <assert.h>
#include <unistd.h>

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

static void ceda_start(void) {
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        void (*start)(void) = modules[i]->start;
        if (start) {
            start();
        }
    }
}

static void ceda_poll(void) {
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        void (*poll)(void) = modules[i]->poll;
        if (poll) {
            poll();
        }
    }
}

static void ceda_remaining(void) {
    us_interval_t wait = LONG_MAX;
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        remaining_handler_t remaining = modules[i]->remaining;
        if (!remaining) {
            continue;
        }
        wait = MIN(remaining(), wait);
    }
    if (wait > 0) {
        usleep((__useconds_t)wait);
    }
}

static void ceda_performance(void) {
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        performance_handler_t perf = modules[i]->performance;
        if (!perf) {
            continue;
        }
        float value;
        const char *unit;
        perf(&value, &unit);
        LOG_DEBUG("module %u: %f %s\n", i, value, unit);
    }
}

static void ceda_cleanup(void) {
    for (int i = ARRAY_SIZE(modules) - 1; i >= 0; --i) {
        void (*cleanup)(void) = modules[i]->cleanup;
        if (cleanup) {
            cleanup();
        }
    }
}

void ceda_run(void) {
    // start all modules
    ceda_start();

    // main loop
    for (;;) {
        // poll all modules
        ceda_poll();

        // decide wether to exit
        if (gui_isQuit() || cli_isQuit()) {
            break;
        }

        // check for how long each module can sleep, and yield host cpu
        ceda_remaining();

        // retrieve and print modules performance metrics
        ceda_performance();
    }

    // cleanup all modules
    ceda_cleanup();
}
