#include "speaker.h"

#include "gui.h"

#include <SDL2/SDL_mixer.h>
#include <stdbool.h>

#define LOG_LEVEL LOG_LVL_DEBUG
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
static Mix_Chunk chunk = {
    .allocated = 0,
    .abuf = sample,
    .alen = SPEAKER_SAMPLE_SIZE,
    .volume = 64,
};

void speaker_init(void) {
    // a square wave
    for (size_t i = 0; i < SPEAKER_SAMPLE_SIZE; ++i) {
        sample[i] = ((i % SPEAKER_SAMPLES_PER_PERIOD) <
                     (SPEAKER_SAMPLES_PER_PERIOD / 2))
                        ? 255
                        : 0;
    }
}

void speaker_start(void) {
    if (!gui_isStarted()) {
        LOG_WARN("no gui: default to terminal speaker\n");
        return;
    }

    if (Mix_OpenAudio(SPEAKER_SAMPLE_RATE, AUDIO_U8, 1, SPEAKER_SAMPLE_SIZE) <
        0) {
        LOG_WARN("unable to open audio card: default to terminal speaker");
        return;
    }

    LOG_INFO("%s: ready\n", __func__);
    fallback = false;
}

zuint8 speaker_in(void *context, zuint16 address) {
    (void)context;
    (void)address;

    speaker_trigger();

    return 0;
}

void speaker_out(void *context, zuint16 address, zuint8 value) {
    (void)context;
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

    Mix_PlayChannel(-1, &chunk, 0);
}