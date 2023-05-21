#include "fdc.h"

void fdc_init(void) {
    // TODO
}

zuint8 fdc_in(void *context, zuint16 address) {
    // TODO
    (void)context;
    (void)address;

    // TODO -- this allows routine $C403 to proceed
    return 0x80;

    return 0;
}

void fdc_out(void *context, zuint16 address, zuint8 value) {
    // TODO
    (void)context;
    (void)address;
    (void)value;
}