#include "sio2.h"

#include <assert.h>

#define SIO2_REG_NUM 4

#define SIO2_CHA_DATA_REG    0x00
#define SIO2_CHA_CONTROL_REG 0x01
#define SIO2_CHB_DATA_REG    0x02
#define SIO2_CHB_CONTROL_REG 0x03

void sio2_init(void) {
    // TODO
}

zuint8 sio2_in(void *context, zuint16 address) {
    assert(address < SIO2_REG_NUM);

    (void)context;

    if (address == SIO2_CHA_DATA_REG) {
        // TODO -- read external RS232
        return 0;
    } else if (address == SIO2_CHA_CONTROL_REG) {
        // TODO
    } else if (address == SIO2_CHB_DATA_REG) {
        // TODO -- read keyboard
        return 0;
    } else if (address == SIO2_CHB_CONTROL_REG) {
        // TODO
        return 0x01; // "character available"
    }

    assert(0);
    return 0x00;
}

void sio2_out(void *context, zuint16 address, zuint8 value) {
    assert(address < SIO2_REG_NUM);

    (void)context;
    (void)address;
    (void)value;
}