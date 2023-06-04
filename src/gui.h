#ifndef CEDA_GUI_H
#define CEDA_GUI_H

#include "module.h"

#include <stdbool.h>

void gui_init(CEDAModule *mod);

bool gui_isStarted(void);
bool gui_isQuit(void);

#endif // CEDA_GUI_H
