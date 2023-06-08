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

uint8_t sio2_in(ceda_ioaddr_t address) {
    assert(address < SIO2_REG_NUM);

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

void sio2_out(ceda_ioaddr_t address, uint8_t value) {
    assert(address < SIO2_REG_NUM);

    (void)address;
    (void)value;
}