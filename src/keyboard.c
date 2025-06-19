#include "keyboard.h"

#include "fifo.h"
#include "macro.h"
#include "video.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_scancode.h>
#include <stdint.h>
#include <string.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

typedef enum ceda_associator_type_t {
    CEDA_ASSOCIATOR_NOP,
    CEDA_ASSOCIATOR_KEY,
    CEDA_ASSOCIATOR_FUNC,
} ceda_associator_type_t;

typedef void (*ceda_associator_func_t)(SDL_Scancode);

typedef struct ceda_associator_t {
    SDL_Scancode sdl;
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
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        modifiers ^= KEYBOARD_MODIFIER_SHIFT;
        break;
    case SDL_SCANCODE_CAPSLOCK:
        modifiers ^= KEYBOARD_MODIFIER_CAPS_LOCK;
        break;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
        modifiers ^= KEYBOARD_MODIFIER_ALT;
        break;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        modifiers ^= KEYBOARD_MODIFIER_CTRL;
        break;
    default:
        break;
    }
}

static const ceda_associator_t associators[] = {
    // row 0
    {SDL_SCANCODE_INSERT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4D}},
    {SDL_SCANCODE_F1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4E}},
    {SDL_SCANCODE_F2, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4F}},
    {SDL_SCANCODE_F3, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x50}},
    {SDL_SCANCODE_F4, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x51}},
    {SDL_SCANCODE_F5, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x52}},
    {SDL_SCANCODE_F6, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x53}},
    {SDL_SCANCODE_F7, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x54}},
    {SDL_SCANCODE_F8, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x55}},
    {SDL_SCANCODE_F9, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x56}},
    {SDL_SCANCODE_F10, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x57}},
    {SDL_SCANCODE_F11, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x58}},
    {SDL_SCANCODE_F12, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x59}},
    {SDL_SCANCODE_F13, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x5A}},
    {SDL_SCANCODE_F14, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x5B}},
    {SDL_SCANCODE_F15, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x5C}},

    // row 1
    {SDL_SCANCODE_ESCAPE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x01}},
    {SDL_SCANCODE_GRAVE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x02}},
    {SDL_SCANCODE_1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x03}},
    {SDL_SCANCODE_2, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x04}},
    {SDL_SCANCODE_3, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x05}},
    {SDL_SCANCODE_4, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x06}},
    {SDL_SCANCODE_5, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x07}},
    {SDL_SCANCODE_6, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x08}},
    {SDL_SCANCODE_7, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x09}},
    {SDL_SCANCODE_8, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0a}},
    {SDL_SCANCODE_9, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0b}},
    {SDL_SCANCODE_0, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0C}},
    {SDL_SCANCODE_MINUS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0D}},
    {SDL_SCANCODE_EQUALS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0E}},
    {SDL_SCANCODE_DELETE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x0F}},
    // {} ex CARET (0x10)
    // mapped to SDL_SCANCODE_NONUSBACKSLASH (additional key for ISO layout)

    // row 2
    // {} BREAK (0x11)
    {SDL_SCANCODE_TAB, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x12}},
    {SDL_SCANCODE_Q, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x13}},
    {SDL_SCANCODE_W, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x14}},
    {SDL_SCANCODE_E, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x15}},
    {SDL_SCANCODE_R, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x16}},
    {SDL_SCANCODE_T, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x17}},
    {SDL_SCANCODE_Y, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x18}},
    {SDL_SCANCODE_U, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x19}},
    {SDL_SCANCODE_I, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1A}},
    {SDL_SCANCODE_O, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1B}},
    {SDL_SCANCODE_P, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1C}},
    {SDL_SCANCODE_LEFTBRACKET, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1D}},
    {SDL_SCANCODE_RIGHTBRACKET, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1E}},
    {SDL_SCANCODE_BACKSLASH, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x1F}},

    // row 3
    {SDL_SCANCODE_CAPSLOCK, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    // {} SHIFT LOCK (0x65)
    {SDL_SCANCODE_A, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x20}},
    {SDL_SCANCODE_S, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x21}},
    {SDL_SCANCODE_D, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x22}},
    {SDL_SCANCODE_F, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x23}},
    {SDL_SCANCODE_G, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x24}},
    {SDL_SCANCODE_H, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x25}},
    {SDL_SCANCODE_J, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x26}},
    {SDL_SCANCODE_K, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x27}},
    {SDL_SCANCODE_L, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x28}},
    {SDL_SCANCODE_SEMICOLON, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x29}},
    {SDL_SCANCODE_APOSTROPHE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2A}},
    {SDL_SCANCODE_RETURN, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2B}},
    // {} ex LEFT BRACKET (0x2C)

    // row 4
    // {} CAN (0x2D)
    {SDL_SCANCODE_LSHIFT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDL_SCANCODE_Z, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2E}},
    {SDL_SCANCODE_X, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x2F}},
    {SDL_SCANCODE_C, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x30}},
    {SDL_SCANCODE_V, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x31}},
    {SDL_SCANCODE_B, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x32}},
    {SDL_SCANCODE_N, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x33}},
    {SDL_SCANCODE_M, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x34}},
    {SDL_SCANCODE_COMMA, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x35}},
    {SDL_SCANCODE_PERIOD, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x36}},
    {SDL_SCANCODE_SLASH, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x37}},
    {SDL_SCANCODE_RSHIFT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    // {} LINE FEED (0x38)

    // row 5
    {SDL_SCANCODE_LCTRL, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDL_SCANCODE_SPACE, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x39}},
    {SDL_SCANCODE_RALT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},

    // useful keys on modern IBM keyboards
    {SDL_SCANCODE_RCTRL, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDL_SCANCODE_LALT, CEDA_ASSOCIATOR_FUNC, keyboard_toggle_modifier},
    {SDL_SCANCODE_NONUSBACKSLASH, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x10}},

    // number pad
    {SDL_SCANCODE_UP, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3A}},
    {SDL_SCANCODE_DOWN, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3B}},
    {SDL_SCANCODE_LEFT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3C}},
    {SDL_SCANCODE_RIGHT, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3D}},
    {SDL_SCANCODE_KP_7, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3E}},
    {SDL_SCANCODE_KP_8, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x3F}},
    {SDL_SCANCODE_KP_9, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x40}},
    {SDL_SCANCODE_KP_CLEAR, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x41}},
    {SDL_SCANCODE_KP_4, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x42}},
    {SDL_SCANCODE_KP_5, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x43}},
    {SDL_SCANCODE_KP_6, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x44}},
    {SDL_SCANCODE_KP_MINUS, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x45}},
    {SDL_SCANCODE_KP_1, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x46}},
    {SDL_SCANCODE_KP_2, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x47}},
    {SDL_SCANCODE_KP_3, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x48}},
    {SDL_SCANCODE_KP_ENTER, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4C}},
    {SDL_SCANCODE_KP_PERIOD, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x49}},
    {SDL_SCANCODE_KP_0, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4A}},
    {SDL_SCANCODE_KP_00, CEDA_ASSOCIATOR_KEY, &(uint8_t){0x4B}},
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
    LOG_DEBUG("scancode = %" PRId32 ", repeat = %d\n", event->keysym.scancode,
              (int)event->repeat);

    for (size_t i = 0; i < ARRAY_SIZE(associators); ++i) {
        const ceda_associator_t *const associator = &associators[i];
        if (associator->sdl != event->keysym.scancode)
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
