#include "gui.h"

#include "time.h"

#include <SDL2/SDL.h>

#include "log.h"

static bool started = false;
static bool quit = false;
static long last_update = 0;
static SDL_Event event;

void gui_init(void) {
    /* nop */
}

void gui_start(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        LOG_ERR("unable to initialize SDL: %s\n", SDL_GetError());
        abort();
    }

    started = true;
}

bool gui_isStarted(void) {
    return started;
}

void gui_pollEvent(void) {
    last_update = time_now_ms();

    SDL_PollEvent(&event);

    quit = (event.type == SDL_QUIT);
}

long gui_remaining(void) {
#define UPDATE_INTERVAL 20 // [ms] 20 ms => 50 Hz
    const long now = time_now_ms();
    const long next_update = last_update + UPDATE_INTERVAL;
    const long diff = next_update - now;
    return diff;
}

bool gui_isQuit(void) {
    return quit;
}

void gui_cleanup(void) {
    SDL_Quit();
}
