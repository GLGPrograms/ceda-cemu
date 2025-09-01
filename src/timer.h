#ifndef CEDA_TIMER_H
#define CEDA_TIMER_H

#include "module.h"
#include "type.h"

#include <Z80.h>

void timer_init(CEDAModule *mod);

uint8_t timer_in(ceda_ioaddr_t address);
void timer_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_TIMER_H
