#include "sio2.h"

#include "keyboard.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#define SIO2_REG_NUM 4

#define SIO2_CHA_DATA_REG    (0x00)
#define SIO2_CHA_CONTROL_REG (0x01)
#define SIO2_CHB_DATA_REG    (0x02)
#define SIO2_CHB_CONTROL_REG (0x03)

typedef struct sio_device_t {
    uint8_t control;
    uint8_t data;
    bool dav; // data available
} sio_device_t;

#define SIO2_DEVICES (2)
static sio_device_t devices[SIO2_DEVICES];

void sio2_init(void) {
    for (size_t i = 0; i < countof(devices); ++i) {
        memset(&devices[i], 0, sizeof(devices[i]));
    }
}

uint8_t sio2_in(ceda_ioaddr_t address) {
    assert(address < SIO2_REG_NUM);

    if (address == SIO2_CHA_DATA_REG) {
        // TODO(giomba): read external RS232
        return 0;
    }
    if (address == SIO2_CHA_CONTROL_REG) {
        // TODO(giomba)
        return 0;
    }
    if (address == SIO2_CHB_DATA_REG) {
        devices[1].dav = false;
        devices[1].control &= ~0x01;
        return devices[1].data;
    }
    if (address == SIO2_CHB_CONTROL_REG) {
        if (devices[1].dav)
            return devices[1].control;

        char c = '\0';
        const bool ok = keyboard_getChar(&c);

        if (!ok)
            return devices[1].control;

        devices[1].data = c;
        devices[1].dav = true;
        devices[1].control |= 0x01; // signal data available
        return devices[1].control;
    }

    assert(0);
    return 0x00;
}

void sio2_out(ceda_ioaddr_t address, uint8_t value) {
    assert(address < SIO2_REG_NUM);

    (void)address;
    (void)value;
}