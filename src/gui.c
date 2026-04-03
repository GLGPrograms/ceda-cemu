#include "gui.h"

#include "keyboard.h"
#include "module.h"
#include "time.h"

#include <string.h>

#include <SDL3/SDL.h>

#include "log.h"

static bool started = false;
static bool quit = false;
#define UPDATE_INTERVAL 20000 // [us] 20 ms => 50 Hz
static us_time_t last_update = 0;

bool gui_isStarted(void) {
    return started;
}

bool gui_isQuit(void) {
    return quit;
}

static bool gui_start(void) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        LOG_ERR("unable to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    started = true;
    return true;
}

static void gui_poll(void) {
    last_update = time_now_us();

    SDL_Event event;
    if (!SDL_PollEvent(&event))
        return;

    quit = (event.type == SDL_EVENT_QUIT);

    // handle keyboard events
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const SDL_KeyboardEvent *key_event =
            (const SDL_KeyboardEvent *)&event.key;
        keyboard_handleEvent(key_event);
    }
}

static long gui_remaining(void) {
    const us_time_t now = time_now_us();
    const us_time_t next_update = last_update + UPDATE_INTERVAL;
    const us_time_t diff = next_update - now;
    return diff;
}

static void gui_cleanup(void) {
    if (!started)
        return;

    SDL_Quit();
}

void gui_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = gui_init;
    mod->start = gui_start;
    mod->poll = gui_poll;
    mod->remaining = gui_remaining;
    mod->cleanup = gui_cleanup;

    keyboard_init();
}
