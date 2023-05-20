#ifndef CEDA_PLAIN_RAM
#define CEDA_PLAIN_RAM

#include <Z80.h>

typedef MemoryDevice {
    Z80Read read;
    Z80Write write;
} MemoryDevice;

#endif // CEDA_PLAIN_RAM

