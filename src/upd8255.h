#ifndef CEDA_UPD8255_H
#define CEDA_UPD8255_H

#include "type.h"

#include <Z80.h>

void upd8255_init(void);

uint8_t upd8255_in(ceda_ioaddr_t address);
void upd8255_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_UPD_8255_H
