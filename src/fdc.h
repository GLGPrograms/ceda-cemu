#ifndef CEDA_FDC_H
#define CEDA_FDC_H

#include <Z80.h>

void fdc_init(void);

zuint8 fdc_in(void *context, zuint16 address);
void fdc_out(void *context, zuint16 address, zuint8 value);

#endif // CEDA_FDC_H
