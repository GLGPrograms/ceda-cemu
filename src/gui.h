#ifndef CEDA_GUI_H
#define CEDA_GUI_H

#include <stdbool.h>

void gui_init(void);
void gui_start(void);
void gui_poll(void);
long gui_remaining(void);
void gui_cleanup(void);

bool gui_isStarted(void);
bool gui_isQuit(void);

#endif // CEDA_GUI_H
