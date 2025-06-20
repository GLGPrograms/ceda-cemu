#include "video.h"

#include "conf.h"
#include "crtc.h"
#include "gui.h"
#include "macro.h"
#include "time.h"
#include "units.h"

#include <SDL2/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

#define VIDEO_CHAR_MEM_SIZE 0x800
#define VIDEO_ATTR_MEM_SIZE VIDEO_CHAR_MEM_SIZE
#define VIDEO_COLUMNS       80
#define VIDEO_ROWS          25

#define CRT_PIXEL_WIDTH  640
#define CRT_PIXEL_HEIGHT 400

#define CHAR_ROM_PATH "rom/CGV7.2_ROM.bin"
#define CHAR_ROM_SIZE (ceda_size_t)(4 * KiB)
#define CGE_ROM_PATH  "rom/CGE.bin"
#define CGE_ROM_SIZE  (ceda_size_t)(4 * KiB)

#define UPDATE_INTERVAL 20000 // [us] 20 ms => 50 Hz
static us_time_t last_update = 0;

static zuint8 mem_char[VIDEO_CHAR_MEM_SIZE];
static zuint8 mem_attr[VIDEO_ATTR_MEM_SIZE];
static zuint8 *mem = NULL; // pointer to current selected memory bank
static zuint8 char_rom[CHAR_ROM_SIZE];
static zuint8 cge_rom[CGE_ROM_SIZE];
static bool cge_installed = false;

static SDL_Window *window = NULL;
static SDL_Surface *surface = NULL;
static SDL_Renderer *renderer = NULL;
static bool started = false;

static float perf_value = 0;
static const char *perf_unit = "fps";

static unsigned long int fields = 0; // displayed video fields

static bool frame_sync = false; // set to true for each new frame

static bool video_load_roms(void) {
    // load character generator rom
    {
        const char *rom_path = CHAR_ROM_PATH;
        const char *rom_path_cfg = conf_getString("path", "char_rom");

        if (rom_path_cfg != NULL)
            rom_path = rom_path_cfg;

        LOG_INFO("Loading char rom from %s\n", rom_path);

        FILE *fp = fopen(rom_path, "rb");

        if (fp == NULL) {
            LOG_ERR("missing char rom file\n");
            return false;
        }

        const size_t read = fread(char_rom, 1, CHAR_ROM_SIZE, fp);
        if (read != CHAR_ROM_SIZE) {
            LOG_ERR("bad char rom file size: %lu\n", read);
            return false;
        }

        if (fclose(fp) != 0) {
            LOG_ERR("error closing char rom file\n");
            return false;
        }
    }

    // load extended character generator rom (custom mod)
    bool *conf_cge_installed = conf_getBool("mod", "cge_installed");
    if (conf_cge_installed)
        cge_installed = *conf_cge_installed;

    if (cge_installed) {
        const char *rom_path = CGE_ROM_PATH;
        const char *rom_path_cfg = conf_getString("path", "cge_rom");

        if (rom_path_cfg != NULL)
            rom_path = rom_path_cfg;

        LOG_INFO("Loading CGE rom from %s\n", rom_path);

        FILE *fp = fopen(rom_path, "rb");

        if (fp == NULL) {
            LOG_WARN("cge: extended char rom not found\n");
            return true;
        }

        const size_t read = fread(cge_rom, 1, CGE_ROM_SIZE, fp);
        if (read != CGE_ROM_SIZE) {
            LOG_WARN("cge: extended character rom found, but cannot read\n");
            return true;
        }

        if (fclose(fp) != 0) {
            LOG_ERR("cge: error closing extended char rom file\n");
            return false;
        }

        LOG_INFO("cge: mod installed ok\n");
    }

    return true;
}

static bool video_start(void) {
    if (!gui_isStarted())
        return false;

    if (!video_load_roms())
        return false;

    window = SDL_CreateWindow("ceda cemu", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, CRT_PIXEL_WIDTH,
                              CRT_PIXEL_HEIGHT,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (window == NULL) {
        LOG_ERR("unable to create window: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        LOG_ERR("unable to create renderer: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowMinimumSize(window, CRT_PIXEL_WIDTH, CRT_PIXEL_HEIGHT);
    if (SDL_RenderSetLogicalSize(renderer, CRT_PIXEL_WIDTH, CRT_PIXEL_HEIGHT) <
        0) {
        LOG_ERR("sdl error: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_RenderSetIntegerScale(renderer, SDL_TRUE) < 0) {
        LOG_ERR("sdl error: %s\n", SDL_GetError());
        return false;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, CRT_PIXEL_WIDTH,
                                             CRT_PIXEL_HEIGHT, 1,
                                             SDL_PIXELFORMAT_INDEX1MSB);
    SDL_Color colors[2] = {{0, 0, 0, 255}, {0, 192, 0, 255}};
    SDL_SetPaletteColors(surface->format->palette, colors, 0, 2);

    started = true;
    return true;
}

bool video_isStarted(void) {
    return started;
}

static void video_performance(float *value, const char **unit) {
    *value = perf_value;
    *unit = perf_unit;
}

static void video_update_performance(void) {
    static unsigned long int last_fields = 0;
    static us_time_t last_time = 0;

    const us_time_t now = time_now_us();

    const us_time_t diff_utime = now - last_time;
    const unsigned long int diff_fields = fields - last_fields;

    perf_value = (float)diff_fields / ((float)diff_utime / 1000.0F / 1000.0F);

    last_time = now;
    last_fields = fields;
}

static void video_poll(void) {
    const us_time_t now = time_now_us();
    if (now < last_update + UPDATE_INTERVAL)
        return;
    last_update = now;

    if (!started)
        return;

    ++fields;
    frame_sync = true;

    // get CRTC base address
    const uint16_t crtc_start_address = crtc_startAddress();

    // get base pointer of SDL surface bitmap
    zuint8 *pixels = (zuint8 *)(surface->pixels);

    for (size_t row = 0; row < VIDEO_ROWS; ++row) {
        for (size_t column = 0; column < VIDEO_COLUMNS; ++column) {
            // get character at (row,column) position in video memory, and its
            // attributes
            const unsigned char c = (unsigned char)
                mem_char[(crtc_start_address + row * VIDEO_COLUMNS + column) %
                         ARRAY_SIZE(mem_char)];
            const zuint8 attr =
                mem_attr[(crtc_start_address + row * VIDEO_COLUMNS + column) %
                         ARRAY_SIZE(mem_attr)];

            const zuint8 *selected_char_rom =
                (cge_installed) ? ((attr & 0x80) ? cge_rom : char_rom)
                                : char_rom;

            // pointer to bitmap in the char rom,
            // for the character we need to draw
            const zuint8 *bitmap = selected_char_rom + (ptrdiff_t)c * 16;

            // need to stretch char horizontally?
            const bool hstretch = attr & 0x08;

            // draw the 16 lines which compose the character on the screen
            // this does not emulate 100% the CRTC scan lines, but it's easier
            // and no one cares (yet)
            for (int i = 0; i < 16; ++i) {
                // compute pointer to SDL frame buffer memory where char will
                // reside
                zuint8 *pixels_segment = pixels + (row * 16) * VIDEO_COLUMNS +
                                         column + (ptrdiff_t)i * VIDEO_COLUMNS;

                // retrieve i-th horizontal segment which composes the gliph,
                // from the char rom
                zuint8 segment = bitmap[i];

                // index 28L22 glue ROM for special chars effects (emulated)
                const zuint8 attr_index = (attr >> 4) & 0x7;
                switch (attr_index) {
                // plain gliph from char ROM
                case 0:
                    break;

                // enable underline when line 13 comes
                case 1:
                    if (i == 0xd)
                        segment = 0xff;
                    break;

                // enable underline when line 13 comes, but also blink
                case 2:
                    if (i == 0xd)
                        segment = (fields % 32 < 16) ? 0x0 : 0xff;
                    break;

                // enable overline when line 0 comes
                case 3:
                    if (i == 0)
                        segment = 0xff;
                    break;

                // hide
                case 4:
                    segment = 0;
                    break;

                // enable underline and overline
                case 5:
                    if (i == 0x0 || i == 0xd)
                        segment = 0xff;
                    break;

                // enable vertical stretch (upper part)
                case 6:
                    segment = bitmap[i / 2];
                    break;

                // enable vertical stretch (lower part)
                case 7:
                    if (i <= 6 * 2)
                        segment = bitmap[7 + i / 2];
                    else
                        segment = 0;
                    break;

                default:
                    assert(false);
                }

                // mangle segment depending on text attribute, if any
                if (attr) {
                    // invert colors
                    if (attr & 0x01)
                        segment ^= 0xff;

                    // blink
                    if (attr & 0x02) {
                        if (fields % 32 < 16)
                            segment = 0;
                    }

                    // unknown / cursor? attribute?
                    if (attr & 0x04) {
                        ;
                    }

                    // horizontal stretch
                    if (hstretch) {
                        // compute widened char segment
                        zuint16 wide_segment = 0;
                        for (int i = 7; i >= 0; --i) {
                            const bool lit = segment & (1 << i);
                            wide_segment |= (zuint16)((lit ? 3 : 0) << (i * 2));
                        }
                        *pixels_segment = (zuint8)((wide_segment >> 8) & 0xff);
                        segment = (zuint8)(wide_segment & 0xff);
                        ++pixels_segment;
                    }
                }
                *pixels_segment = segment;
            }
            // stretch implies skipping char on your right
            if (hstretch)
                ++column;
        }
    }

    // update cursor on screen
    const unsigned int cursor_position =
        crtc_cursorPosition() - crtc_startAddress();
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
    uint8_t cursor_raster_start;
    uint8_t cursor_raster_end;
    crtc_cursorRasterSize(&cursor_raster_start, &cursor_raster_end);
    if (blink_period == 0 || ((fields % blink_period) < (blink_period / 2))) {
        for (uint8_t raster = cursor_raster_start; raster <= cursor_raster_end;
             ++raster) {
            *(pixels + ((ptrdiff_t)row * 16) * VIDEO_COLUMNS + column +
              (ptrdiff_t)raster * VIDEO_COLUMNS) ^= 0xff;
        }
    }

    // render
    SDL_RenderClear(renderer);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);
    SDL_UpdateWindowSurface(window);

    // measure performance
    video_update_performance();
}

static us_time_t video_remaining(void) {
    const us_time_t now = time_now_us();
    const us_time_t next_update = last_update + UPDATE_INTERVAL;
    const us_time_t diff = next_update - now;
    return diff;
}

void video_init(CEDAModule *mod) {
    // mod init
    memset(mod, 0, sizeof(*mod));
    mod->init = video_init;
    mod->start = video_start;
    mod->poll = video_poll;
    mod->remaining = video_remaining;
    mod->cleanup = NULL;
    mod->performance = video_performance;

    // default to character memory
    mem = mem_char;
}

zuint8 video_ram_read(ceda_address_t address) {
    assert(address < VIDEO_CHAR_MEM_SIZE);

    return mem[address];
}

void video_ram_write(ceda_address_t address, uint8_t value) {
    assert(address < VIDEO_CHAR_MEM_SIZE);

    LOG_DEBUG("write [%04x] <= %02x\n", address, value);

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

/**
 * @brief Reset video frame sync circuit.
 *
 * See schematics, 74109 JK in L9.
 *
 */
void video_frameSyncReset(void) {
    frame_sync = 0;
}

/**
 * @brief Get current frame sync status.
 *
 * @return Return true when new frame sync since last reset, false otherwise.
 */
bool video_frameSync(void) {
    return frame_sync;
}
