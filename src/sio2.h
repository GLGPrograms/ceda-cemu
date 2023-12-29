#ifndef CEDA_SIO2_H
#define CEDA_SIO2_H

#include "module.h"
#include "type.h"

#include <Z80.h>

void sio2_init(CEDAModule *mod);
uint8_t sio2_in(ceda_ioaddr_t address);
void sio2_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_SIO2_H
