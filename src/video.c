#include "video.h"

#include <SDL2/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

#define VIDEO_CHAR_MEM_SIZE 0x1000
#define VIDEO_ATTR_MEM_SIZE VIDEO_CHAR_MEM_SIZE

#define CRT_PIXEL_WIDTH  640
#define CRT_PIXEL_HEIGHT 400

static zuint8 mem_char[VIDEO_CHAR_MEM_SIZE];
static zuint8 mem_attr[VIDEO_ATTR_MEM_SIZE];
static zuint8 *mem = NULL; // pointer to current selected memory bank

static SDL_Window *window = NULL;
static SDL_Surface *surface = NULL;
static bool started = false;

void video_init(void) {
    mem = mem_char;
}

void video_start(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERR("unable to initialize video: %s\n", SDL_GetError());
        abort();
    }

    window = SDL_CreateWindow("ceda cemu", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, CRT_PIXEL_WIDTH,
                              CRT_PIXEL_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        LOG_ERR("unable to create window: %s\n", SDL_GetError());
    }

    surface = SDL_GetWindowSurface(window);
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0x00, 0x7f, 0x00));

    started = true;
}

void video_update(void) {
    if (!started)
        return;

#define UPDATE_INTERVAL 20 // [ms] (20ms = 50Hz)
    static struct timeval last;

    struct timeval now;
    gettimeofday(&now, NULL);
    const unsigned long diff_s = now.tv_sec - last.tv_sec;
    const unsigned long diff_u = now.tv_usec - last.tv_usec;
    const unsigned long diff_us = diff_s * 1000000UL + diff_u;
    const unsigned long diff_ms = diff_us / 1000;
    if (diff_ms < 20)
        return;

    SDL_UpdateWindowSurface(window);
    last = now;
}

zuint8 video_ram_read(void *context, zuint16 address) {
    (void)context;
    assert(address < VIDEO_CHAR_MEM_SIZE);

    // TODO: handle bank switching
    return mem[address];
}

void video_ram_write(void *context, zuint16 address, zuint8 value) {
    (void)context;
    assert(address < VIDEO_CHAR_MEM_SIZE);

    LOG_DEBUG("video mem write: [%04x] <= %02x\n", address, value);

    mem[address] = value;
}

/**
 * @brief Change video memory bank.
 *
 * @param attr True if attribute bank, false if char bank.
 */
void video_bank(bool attr) {
    if (attr) {
        mem = mem_attr;
    } else {
        mem = mem_char;
    }
}
