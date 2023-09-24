#include "fdc.h"

void fdc_init(void) {
    // TODO(giomba): to be implemented
}

uint8_t fdc_in(ceda_ioaddr_t address) {
    // TODO(giomba): to be implemented
    (void)address;

    // TODO(giomba) -- this allows routine $C403 to proceed
    return 0x80;

    return 0;
}

void fdc_out(ceda_ioaddr_t address, uint8_t value) {
    // TODO(giomba): to be implemented
    (void)address;
    (void)value;
}