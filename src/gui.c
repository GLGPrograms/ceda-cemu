#include "gui.h"

#include <SDL2/SDL.h>

#include "log.h"

static bool started = false;
static bool quit = false;
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
    SDL_PollEvent(&event);

    quit = (event.type == SDL_QUIT);
}

bool gui_isQuit(void) {
    return quit;
}

void gui_cleanup(void) {
    SDL_Quit();
}
