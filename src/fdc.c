#include "fdc.h"

void fdc_init(void) {
    // TODO
}

uint8_t fdc_in(ceda_ioaddr_t address) {
    // TODO
    (void)address;

    // TODO -- this allows routine $C403 to proceed
    return 0x80;

    return 0;
}

void fdc_out(ceda_ioaddr_t address, uint8_t value) {
    // TODO
    (void)address;
    (void)value;
}