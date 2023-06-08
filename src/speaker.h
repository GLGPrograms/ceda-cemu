#ifndef CEDA_SPEAKER_H
#define CEDA_SPEAKER_H

#include "module.h"
#include "type.h"

#include <Z80.h>

void speaker_init(CEDAModule *mod);

uint8_t speaker_in(ceda_ioaddr_t address);
void speaker_out(ceda_ioaddr_t address, uint8_t value);

void speaker_trigger(void);

#endif // CEDA_SPEAKER_H
