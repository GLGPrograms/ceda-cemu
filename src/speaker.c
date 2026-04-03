#include "speaker.h"

#include "gui.h"
#include "module.h"
#include "type.h"

#include <SDL3_mixer/SDL_mixer.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

// fallback mode, a.k.a. your actual terminal speaker
static bool fallback = true;

#define SPEAKER_BEEP_FREQUENCY  1300 // [Hz]
#define SPEAKER_SAMPLE_RATE     8000 // [Hz]
#define SPEAKER_SAMPLE_DURATION 120  // [ms]

#define SPEAKER_SAMPLE_SIZE                                                    \
    (SPEAKER_SAMPLE_DURATION * SPEAKER_SAMPLE_RATE / 1000)
#define SPEAKER_SAMPLES_PER_PERIOD                                             \
    (SPEAKER_SAMPLE_RATE / SPEAKER_BEEP_FREQUENCY)

static uint8_t sample[SPEAKER_SAMPLE_SIZE] = {0};

static MIX_Mixer *mixer = NULL;
static MIX_Track *track = NULL;
static MIX_Audio *audio = NULL;

static bool speaker_start(void) {
    if (!gui_isStarted()) {
        LOG_WARN("speaker: no GUI started, aborting audio init\n");
        return false;
    }

    if (!MIX_Init()) {
        LOG_WARN("unable to initialize SDL mixer: %s\n", SDL_GetError());
        LOG_INFO("default to terminal speaker\n");
        return true;
    }

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!mixer) {
        LOG_WARN("unable to create mixer: %s\n", SDL_GetError());
        LOG_INFO("default to terminal speaker\n");
        return true;
    }

    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_U8,
        .channels = 1,
        .freq = SPEAKER_SAMPLE_RATE,
    };

    audio = MIX_LoadRawAudio(NULL, sample, sizeof(sample), &spec);
    if (!audio) {
        LOG_WARN("unable to load raw audio: %s\n", SDL_GetError());
        LOG_INFO("default to terminal speaker\n");
        return true;
    }

    track = MIX_CreateTrack(mixer);
    if (!track) {
        LOG_WARN("unable to create track: %s\n", SDL_GetError());
        LOG_INFO("default to terminal speaker\n");
        return true;
    }

    if (!MIX_SetTrackAudio(track, audio)) {
        LOG_WARN("unable to set track audio: %s\n", SDL_GetError());
        LOG_INFO("default to terminal speaker\n");
        return true;
    }

    LOG_INFO("%s: ready\n", __func__);
    fallback = false;
    return true;
}

void speaker_init(CEDAModule *mod) {
    // init mod struct
    memset(mod, 0, sizeof(*mod));
    mod->init = speaker_init;
    mod->start = speaker_start;
    mod->poll = NULL;
    mod->remaining = NULL;
    mod->cleanup = NULL;

    // a square wave
    for (size_t i = 0; i < SPEAKER_SAMPLE_SIZE; ++i) {
        sample[i] = ((i % SPEAKER_SAMPLES_PER_PERIOD) <
                     (SPEAKER_SAMPLES_PER_PERIOD / 2))
                        ? 255
                        : 0;
    }
}

uint8_t speaker_in(ceda_ioaddr_t address) {
    (void)address;

    speaker_trigger();

    return 0;
}

void speaker_out(ceda_ioaddr_t address, uint8_t value) {
    (void)address;
    (void)value;

    speaker_trigger();
}

void speaker_trigger(void) {
    LOG_DEBUG("%s\n", __func__);

    if (fallback) {
#define BELL 0x07
        printf("%c", BELL);
        return;
    }

    if (!MIX_PlayTrack(track, 0))
        LOG_WARN("unable to play track: %s\n", SDL_GetError());
}
