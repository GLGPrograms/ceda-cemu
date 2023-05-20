#ifndef CEDA_CRTC_H
#define CEDA_CRTC_H

#include <Z80.h>

void crtc_init(void);

zuint8 crtc_in(void *context, zuint16 address);
void crtc_out(void *context, zuint16 address, zuint8 value);

#endif // CEDA_CRTC_H
