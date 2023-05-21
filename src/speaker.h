#ifndef CEDA_SPEAKER_H
#define CEDA_SPEAKER_H

#include <Z80.h>

void speaker_init(void);
void speaker_start(void);

zuint8 speaker_in(void *context, zuint16 address);
void speaker_out(void *context, zuint16 address, zuint8 value);

void speaker_trigger(void);

#endif // CEDA_SPEAKER_H
