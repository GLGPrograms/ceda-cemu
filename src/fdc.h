#ifndef CEDA_FDC_H
#define CEDA_FDC_H

#include "type.h"

#include <Z80.h>

void fdc_init(void);

uint8_t fdc_in(ceda_ioaddr_t address);
void fdc_out(ceda_ioaddr_t address, uint8_t value);
void fdc_tc_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_FDC_H
