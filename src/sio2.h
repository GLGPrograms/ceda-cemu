#ifndef CEDA_SIO2_H
#define CEDA_SIO2_H

#include <Z80.h>

void sio2_init(void);
zuint8 sio2_in(void *context, zuint16 address);
void sio2_out(void *context, zuint16 address, zuint8 value);

#endif // CEDA_SIO2_H
