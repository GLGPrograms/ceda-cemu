#include "upd8255.h"

#include "bus.h"
#include "video.h"

#include <assert.h>
#include <stdint.h>

#include "log.h"

#define UPD8255_PORTA_REG 0
#define UPD8255_PORTB_REG 1
#define UPD8255_PORTC_REG 2

#define UPD8255_PORTS_COUNT 3
uint8_t port[UPD8255_PORTS_COUNT];
#define UPD8255_CONTROL_REG 3

#define UPD8255_REG_COUNT 4

void upd8255_init(void) {
    ;
}

uint8_t upd8255_in(ceda_ioaddr_t address) {
    assert(address < UPD8255_REG_COUNT);

    if (address == UPD8255_CONTROL_REG) {
        // nop - this register can't be read
        return 0;
    }

    if (address == UPD8255_PORTC_REG) {
        uint8_t port_c = 0x00;
        // C0 -- to be implemented

        // C1: CRTC frame sync
        port_c |= (!!video_frameSync()) << 1;

        // Cx -- to be implemented

        return port_c;
    }

    return (zuint8)(port[address]);
}

void upd8255_out(ceda_ioaddr_t address, uint8_t value) {
    assert(address < UPD8255_REG_COUNT);

    if (address == UPD8255_CONTROL_REG) {
        // TODO(giomba): set mode and PORT pins in/out
        return;
    }

    port[address] = value;

    if (address == UPD8255_PORTA_REG) {
        // TODO(giomba): to be implemented
        return;
    }
    if (address == UPD8255_PORTB_REG) {
        bus_memSwitch(value & 0x01);
        // bank 7 switching
        video_bank(value & 0x80);
        return;
    }
    if (address == UPD8255_PORTC_REG) {
        // TODO(giomba): to be implemented
        return;
    }

    assert(0);
}
