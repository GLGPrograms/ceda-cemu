#ifndef CEDA_UPD8255_H
#define CEDA_UPD8255_H

#include <Z80.h>

void upd8255_init(void);

zuint8 upd8255_in(void* context, zuint16 address);
void upd8255_out(void* context, zuint16 address, zuint8 value);

#endif // CEDA_UPD_8255_H

