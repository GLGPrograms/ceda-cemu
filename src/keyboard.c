#include "keyboard.h"

#include "3rd/fifo.h"
#include "macro.h"
#include "video.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

typedef enum ceda_associator_type_t {
    CEDA_ASSOCIATOR_NOP,
    CEDA_ASSOCIATOR_KEY,
    CEDA_ASSOCIATOR_FUNC,
} ceda_associator_type_t;

typedef void (*ceda_associator_func_t)(SDL_Keycode);

typedef struct ceda_associator_t {
    SDL_Keycode sdl;
    ceda_associator_type_t type;
    void *ptr;
} ceda_associator_t;

typedef struct ceda_keystroke_t {
    uint8_t key;
    uint8_t modifiers;
} ceda_keystroke_t;

DECLARE_FIFO_TYPE(uint8_t, keyboard_serial_fifo_t, 8);
static keyboard_serial_fifo_t keyboard_serial_fifo;

#define KEYBOARD_MODIFIERS_DEFAULT  (0xC0)
#define KEYBOARD_MODIFIER_SHIFT     (1 << 0)
#define KEYBOARD_MODIFIER_CAPS_LOCK (1 << 1)
#define KEYBOARD_MODIFIER_ALT       (1 << 2)
#define KEYBOARD_MODIFIER_CTRL      (1 << 3)

static uint8_t modifiers = KEYBOARD_MODIFIERS_DEFAULT;

static void keyboard_toggle_modifier(SDL_Keycode code) {
    switch (code) {
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
        modifiers ^= KEYBOARD_MODIFIER_SHIFT;
        break;
    case SDLK_CAPSLOCK:
        modifiers ^= KEYBOARD_MODIFIER_CAPS_LOCK;
        break;
    case SDLK_LALT:
    case SDLK_RALT:
        modifiers ^= KEYBOARD_MODIFIER_ALT;
        break;
    case SDLK_LCTRL:
    case SDLK_RCTRL:
        modifiers ^= KEYBOARD_MODIFIER_CTRL;
        break;
    }
}

static const ceda_associator_t associators[] = {
    {SDLK_0, CEDA_ASSOCIATOR_NOP, NULL},
    {SDLK_1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x23}},
    {SDLK_LSHIFT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
};

void keyboard_init(void) {
    FIFO_INIT(&keyboard_serial_fifo);
}

void keyboard_handleEvent(const SDL_KeyboardEvent *event) {
    LOG_DEBUG("keycode = %" PRId32 ", repeat = %d\n", event->keysym.sym,
              (int)event->repeat);

    for (size_t i = 0; i < ARRAY_SIZE(associators); ++i) {
        const ceda_associator_t *const associator = &associators[i];
        if (associator->sdl != event->keysym.sym)
            continue;

        switch (associator->type) {
        case CEDA_ASSOCIATOR_NOP:
            break;

        case CEDA_ASSOCIATOR_KEY:
            // ignore key release
            if (event->type == SDL_KEYUP)
                break;

            // ignore if FIFO full
            if (FIFO_FREE(&keyboard_serial_fifo) < 2)
                break;

            // append to keystroke FIFO
            const uint8_t key = *((uint8_t *)associator->ptr);
            FIFO_PUSH(&keyboard_serial_fifo, key);
            FIFO_PUSH(&keyboard_serial_fifo, modifiers);

            break;

        case CEDA_ASSOCIATOR_FUNC:
            // call func
            if (associator->ptr) {
                ceda_associator_func_t func = associator->ptr;
                func(associator->sdl);
            }
            break;

        default:
            assert(0);
        }

        break;
    }
}

bool keyboard_getChar(char *c) {
    if (FIFO_ISEMPTY(&keyboard_serial_fifo))
        return false;

    *c = FIFO_POP(&keyboard_serial_fifo);
    return true;
}
