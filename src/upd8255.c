#include "upd8255.h"

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
    port[UPD8255_PORTC_REG] = 0x02; // CRTC frame sync?
}

zuint8 upd8255_in(void *context, zuint16 address) {
    // TODO
    (void)context;
    assert(address < UPD8255_REG_COUNT);
    LOG_DEBUG("upd8255: io_in: [%02x]\n", (zuint8)address);

    if (address == UPD8255_CONTROL_REG) {
        // nop - this register can't be read
    } else {
        return (zuint8)(port[address]);
    }

    return 0;
}

void upd8255_out(void *context, zuint16 address, zuint8 value) {
    (void)context;
    assert(address < UPD8255_REG_COUNT);
    LOG_DEBUG("upd8255: io_out: [%02x] <= %02x\n", (zuint8)address, value);

    if (address == UPD8255_CONTROL_REG) {
        // TODO -- set mode and PORT pins in/out
        return;
    }

    port[address] = value;

    if (address == UPD8255_PORTA_REG) {
        /* nop */
    } else if (address == UPD8255_PORTB_REG) {
        // bank 7 switching
        video_bank(value & 0x80);
    } else if (address == UPD8255_PORTC_REG) {
        /* nop */
    } else {
        assert(0);
    }
}