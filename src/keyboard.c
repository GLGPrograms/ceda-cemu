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
    {SDLK_ESCAPE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x01}},
    {SDLK_MENU, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4D}}, // BOOT key
    {SDLK_q, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x20}},
    {SDLK_w, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2e}},
    {SDLK_e, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x15}},
    {SDLK_r, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x16}},
    {SDLK_t, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x17}},
    {SDLK_y, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x18}},
    {SDLK_u, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x19}},
    {SDLK_i, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1a}},
    {SDLK_o, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1b}},
    {SDLK_p, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1c}},
    {SDLK_a, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x13}},
    {SDLK_s, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x21}},
    {SDLK_d, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x22}},
    {SDLK_f, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x23}},
    {SDLK_g, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x24}},
    {SDLK_h, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x25}},
    {SDLK_j, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x26}},
    {SDLK_k, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x27}},
    {SDLK_l, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x28}},
    {SDLK_m, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x29}},
    {SDLK_z, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x14}},
    {SDLK_x, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2f}},
    {SDLK_c, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x30}},
    {SDLK_v, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x31}},
    {SDLK_b, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x32}},
    {SDLK_n, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x33}},
    {SDLK_RETURN, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2B}},

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
