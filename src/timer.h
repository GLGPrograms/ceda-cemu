#ifndef CEDA_TIMER_H
#define CEDA_TIMER_H

#include <Z80.h>

void timer_init();

zuint8 timer_in(void *context, zuint16 address);
void timer_out(void *context, zuint16 address, zuint8 value);

#endif // CEDA_TIMER_H
