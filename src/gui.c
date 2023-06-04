#include "gui.h"

#include "time.h"

#include <SDL2/SDL.h>

#include "log.h"

static bool started = false;
static bool quit = false;
static long last_update = 0;
static SDL_Event event;

bool gui_isStarted(void) {
    return started;
}

bool gui_isQuit(void) {
    return quit;
}

static void gui_start(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        LOG_ERR("unable to initialize SDL: %s\n", SDL_GetError());
        abort();
    }

    started = true;
}

static void gui_poll(void) {
    last_update = time_now_ms();

    SDL_PollEvent(&event);

    quit = (event.type == SDL_QUIT);
}

static long gui_remaining(void) {
#define UPDATE_INTERVAL 20 // [ms] 20 ms => 50 Hz
    const long now = time_now_ms();
    const long next_update = last_update + UPDATE_INTERVAL;
    const long diff = next_update - now;
    return diff;
}

static void gui_cleanup(void) {
    SDL_Quit();
}

void gui_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = gui_init;
    mod->start = gui_start;
    mod->poll = gui_poll;
    mod->remaining = gui_remaining;
    mod->cleanup = gui_cleanup;
}
