#ifndef CEDA_KEYBOARD_H
#define CEDA_KEYBOARD_H

#include <SDL2/SDL.h>
#include <stdbool.h>

void keyboard_init(void);

void keyboard_handleEvent(const SDL_KeyboardEvent *event);

bool keyboard_getChar(uint8_t *c);

#endif // CEDA_KEYBOARD_H
