#include "video.h"

#include "crtc.h"
#include "gui.h"
#include "units.h"

#include <SDL2/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

#include "log.h"

#define VIDEO_CHAR_MEM_SIZE 0x1000
#define VIDEO_ATTR_MEM_SIZE VIDEO_CHAR_MEM_SIZE
#define VIDEO_COLUMNS       80
#define VIDEO_ROWS          24

#define CRT_PIXEL_WIDTH  640
#define CRT_PIXEL_HEIGHT 400

#define CHAR_ROM_PATH "rom/CGV7.2_ROM.bin"
#define CHAR_ROM_SIZE (4 * KiB)

static zuint8 mem_char[VIDEO_CHAR_MEM_SIZE];
static zuint8 mem_attr[VIDEO_ATTR_MEM_SIZE];
static zuint8 *mem = NULL; // pointer to current selected memory bank
static zuint8 char_rom[CHAR_ROM_SIZE];

static SDL_Window *window = NULL;
static SDL_Surface *surface = NULL;
static SDL_Renderer *renderer = NULL;
static bool started = false;

static unsigned long int fields = 0; // displayed video fields

void video_init(void) {
    // default to character memory
    mem = mem_char;

    // load character generator rom
    FILE *fp = fopen(CHAR_ROM_PATH, "rb");
    if (fp == NULL) {
        LOG_ERR("missing char rom file\n");
        abort();
    }

    size_t r = fread(char_rom, 1, CHAR_ROM_SIZE, fp);
    if (r != CHAR_ROM_SIZE) {
        LOG_ERR("bad char rom file size: %lu\n", r);
        abort();
    }

    fclose(fp);
}

void video_start(void) {
    assert(gui_isStarted());

    window = SDL_CreateWindow("ceda cemu", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, CRT_PIXEL_WIDTH,
                              CRT_PIXEL_HEIGHT,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (window == NULL) {
        LOG_ERR("unable to create window: %s\n", SDL_GetError());
        abort();
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        LOG_ERR("unable to create renderer: %s\n", SDL_GetError());
        abort();
    }

    SDL_SetWindowMinimumSize(window, CRT_PIXEL_WIDTH, CRT_PIXEL_HEIGHT);
    if (SDL_RenderSetLogicalSize(renderer, CRT_PIXEL_WIDTH, CRT_PIXEL_HEIGHT) <
        0) {
        LOG_ERR("sdl error: %s\n", SDL_GetError());
        abort();
    }
    if (SDL_RenderSetIntegerScale(renderer, SDL_TRUE) < 0) {
        LOG_ERR("sdl error: %s\n", SDL_GetError());
        abort();
    }

    surface = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, CRT_PIXEL_WIDTH,
                                             CRT_PIXEL_HEIGHT, 1,
                                             SDL_PIXELFORMAT_INDEX1LSB);
    SDL_Color colors[2] = {{0, 0, 0, 255}, {0, 192, 0, 255}};
    SDL_SetPaletteColors(surface->format->palette, colors, 0, 2);

    started = true;
}

void video_update(void) {
    if (!started)
        return;

#define UPDATE_INTERVAL 20 // [ms] (20ms = 50Hz)
    static struct timeval last;

    // only update at 50Hz
    struct timeval now;
    gettimeofday(&now, NULL);
    const unsigned long diff_s = now.tv_sec - last.tv_sec;
    const unsigned long diff_u = now.tv_usec - last.tv_usec;
    const unsigned long diff_us = diff_s * 1000000UL + diff_u;
    const unsigned long diff_ms = diff_us / 1000;
    if (diff_ms < 20)
        return;

    ++fields;

    // TODO -- perform attribute checks

    // update characters on screen
    zuint8 *pixels = (zuint8 *)(surface->pixels);
    for (size_t row = 0; row < VIDEO_ROWS; ++row) {
        for (size_t column = 0; column < VIDEO_COLUMNS; ++column) {
            // TODO -- add CRTC base pointer
            const char c = mem_char[row * VIDEO_COLUMNS + column];

            const zuint8 *bitmap = char_rom + c * 16;
            for (int i = 0; i < 16; ++i) {
                const zuint8 segment = bitmap[i];
                *(pixels + (row * 16) * VIDEO_COLUMNS + column +
                  i * VIDEO_COLUMNS) = segment;
            }
        }
    }

    const unsigned int cursor_position = crtc_cursorPosition();
    const unsigned int row = cursor_position / VIDEO_COLUMNS;
    const unsigned int column = cursor_position % VIDEO_COLUMNS;
    unsigned int blink_period = 0; // [fields]
    switch (crtc_cursorBlink()) {
    case CRTC_CURSOR_SOLID:
        blink_period = 0;
        break;
    case CRTC_CURSOR_BLINK_FAST:
        blink_period = 16;
        break;
    case CRTC_CURSOR_BLINK_SLOW:
        blink_period = 32;
        break;
    default:
        assert(0);
    }
    uint8_t cursor_raster_start, cursor_raster_end;
    crtc_cursorRasterSize(&cursor_raster_start, &cursor_raster_end);
    if (blink_period == 0 || ((fields % blink_period) < (blink_period / 2))) {
        for (uint8_t raster = cursor_raster_start; raster <= cursor_raster_end;
             ++raster) {
            *(pixels + (row * 16) * VIDEO_COLUMNS + column +
              raster * VIDEO_COLUMNS) = 0xff;
        }
    }

    SDL_RenderClear(renderer);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);

    // draw
    SDL_UpdateWindowSurface(window);

    last = now;
}

zuint8 video_ram_read(void *context, zuint16 address) {
    (void)context;
    assert(address < VIDEO_CHAR_MEM_SIZE);

    return mem[address];
}

void video_ram_write(void *context, zuint16 address, zuint8 value) {
    (void)context;
    assert(address < VIDEO_CHAR_MEM_SIZE);

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
