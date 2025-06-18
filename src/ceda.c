#include "ceda.h"

#include "bios.h"
#include "bus.h"
#include "cli.h"
#include "conf.h"
#include "cpu.h"
#include "fdc.h"
#include "gui.h"
#include "int.h"
#include "limits.h"
#include "macro.h"
#include "module.h"
#include "serial.h"
#include "sio2.h"
#include "speaker.h"
#include "upd8255.h"
#include "video.h"

#include <assert.h>
#include <unistd.h>

#include "log.h"

static CEDAModule mod_bus;
static CEDAModule mod_cpu;
static CEDAModule mod_cli;
static CEDAModule mod_gui;
static CEDAModule mod_video;
static CEDAModule mod_speaker;
static CEDAModule mod_sio2;
static CEDAModule mod_int;
static CEDAModule mod_serial;

static CEDAModule *modules[] = {
    &mod_cli,     &mod_gui, &mod_bus,    &mod_cpu,  &mod_video,
    &mod_speaker, &mod_int, &mod_serial, &mod_sio2,
};

void ceda_init(void) {
    conf_init();
    cli_init(&mod_cli);
    gui_init(&mod_gui);

    fdc_init();
    upd8255_init();
    rom_bios_init();
    video_init(&mod_video);
    speaker_init(&mod_speaker);
    bus_init(&mod_bus);
    cpu_init(&mod_cpu);
    int_init(&mod_int);
    serial_init(&mod_serial);
    sio2_init(&mod_sio2);
}

static bool ceda_start(void) {
    for (unsigned int i = 0; i < ARRAY_SIZE(modules); ++i) {
        bool (*start)(void) = modules[i]->start;
        if (start) {
            bool ok = start();
            if (!ok)
                return false;
        }
    }
    return true;
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

int ceda_run(void) {
    int ret = 0;

    // acquire dynamic resources for all modules
    if (!ceda_start()) {
        LOG_ERR("cannot acquire dynamic resource\n");
        ret = 1;
        goto err;
    }

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

err:
    // cleanup all modules
    ceda_cleanup();
    conf_cleanup();

    return ret;
}
