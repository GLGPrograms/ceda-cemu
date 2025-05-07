#ifndef CEDA_CHAR_MONITOR_H
#define CEDA_CHAR_MONITOR_H

#include "module.h"
#include "type.h"

void charmon_init(CEDAModule *mod);
void charmon_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_CHAR_MONITOR_H
