#include "keyboard.h"

#include "fifo.h"
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
    default:
        break;
    }
}

static const ceda_associator_t associators[] = {
    // row 0
    {SDLK_INSERT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4D}},
    {SDLK_F1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4E}},
    {SDLK_F2, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4F}},
    {SDLK_F3, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x50}},
    {SDLK_F4, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x51}},
    {SDLK_F5, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x52}},
    {SDLK_F6, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x53}},
    {SDLK_F7, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x54}},
    {SDLK_F8, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x55}},
    {SDLK_F9, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x56}},
    {SDLK_F10, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x57}},
    {SDLK_F11, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x58}},
    {SDLK_F12, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x59}},
    {SDLK_F13, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x5A}},
    {SDLK_F14, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x5B}},
    {SDLK_F15, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x5C}},

    // row 1
    {SDLK_ESCAPE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x01}},
    {SDLK_LESS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x02}},
    {SDLK_1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x03}},
    {SDLK_2, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x04}},
    {SDLK_3, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x05}},
    {SDLK_4, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x06}},
    {SDLK_5, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x07}},
    {SDLK_6, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x08}},
    {SDLK_7, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x09}},
    {SDLK_8, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0a}},
    {SDLK_9, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0b}},
    {SDLK_0, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0C}},
    {SDLK_HASH, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0D}},
    {SDLK_AT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0E}},
    {SDLK_DELETE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0F}},
    {SDLK_CARET, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x10}},

    // row 2
    // {} BREAK (0x11)
    {SDLK_TAB, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x12}},
    {SDLK_a, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x13}},
    {SDLK_z, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x14}},
    {SDLK_e, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x15}},
    {SDLK_r, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x16}},
    {SDLK_t, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x17}},
    {SDLK_y, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x18}},
    {SDLK_u, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x19}},
    {SDLK_i, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1A}},
    {SDLK_o, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1B}},
    {SDLK_p, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1C}},
    {SDLK_GREATER, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1D}},
    // {} umlaut / dieresis (0x1E)
    {SDLK_KP_RIGHTBRACE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1F}},

    // row 3
    {SDLK_CAPSLOCK, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    // {} SHIFT LOCK
    {SDLK_q, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x20}},
    {SDLK_s, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x21}},
    {SDLK_d, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x22}},
    {SDLK_f, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x23}},
    {SDLK_g, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x24}},
    {SDLK_h, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x25}},
    {SDLK_j, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x26}},
    {SDLK_k, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x27}},
    {SDLK_l, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x28}},
    {SDLK_m, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x29}},
    {SDLK_PERCENT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2A}},
    {SDLK_RETURN, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2B}},
    {SDLK_KP_LEFTBRACE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2C}},

    // row 4
    // {} CAN (0x2D)
    {SDLK_LSHIFT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDLK_w, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2E}},
    {SDLK_x, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2F}},
    {SDLK_c, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x30}},
    {SDLK_v, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x31}},
    {SDLK_b, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x32}},
    {SDLK_n, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x33}},
    {SDLK_SLASH, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x34}},
    {SDLK_PERIOD, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x35}},
    {SDLK_MINUS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x36}},
    {SDLK_PLUS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x37}},
    {SDLK_RSHIFT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    // {} LINE FEED (0x38)

    // row 5
    {SDLK_LCTRL, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDLK_SPACE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x39}},
    {SDLK_RALT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},

    // useful keys on modern IBM keyboards
    {SDLK_RCTRL, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDLK_LALT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},

    // number pad
    {SDLK_UP, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3A}},
    {SDLK_DOWN, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3B}},
    {SDLK_LEFT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3C}},
    {SDLK_RIGHT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3D}},
    {SDLK_KP_7, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3E}},
    {SDLK_KP_8, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3F}},
    {SDLK_KP_9, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x40}},
    {SDLK_KP_CLEAR, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x41}},
    {SDLK_KP_4, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x42}},
    {SDLK_KP_5, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x43}},
    {SDLK_KP_6, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x44}},
    {SDLK_KP_MINUS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x45}},
    {SDLK_KP_1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x46}},
    {SDLK_KP_2, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x47}},
    {SDLK_KP_3, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x48}},
    {SDLK_KP_ENTER, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4C}},
    {SDLK_KP_PERIOD, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x49}},
    {SDLK_KP_0, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4A}},
    {SDLK_KP_00, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4B}},
};

void keyboard_init(void) {
    FIFO_INIT(&keyboard_serial_fifo);

    // Insert some NUL chars in the FIFO,
    // to trick the BIOS routines which reset the SIO/2
    // by flushing its FIFOs by reading 3 chars.
    for (int i = 0; i < 4; ++i)
        FIFO_PUSH(&keyboard_serial_fifo, 0);
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
            LOG_DEBUG("append to keystroke FIFO\n");
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

bool keyboard_getChar(uint8_t *c) {
    if (FIFO_ISEMPTY(&keyboard_serial_fifo))
        return false;

    *c = FIFO_POP(&keyboard_serial_fifo);
    return true;
}
