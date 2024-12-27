#ifndef CEDA_ALT_RAM_H
#define CEDA_ALT_RAM_H

#include <Z80.h>

zuint8 auxram_read(zuint16 address);
void auxram_write(zuint16 address, zuint8 value);

#endif // CEDA_ALT_RAM_H
