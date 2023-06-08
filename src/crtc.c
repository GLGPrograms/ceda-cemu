#include "crtc.h"

#include <assert.h>
#include <stdint.h>

#define LOG_LEVEL  LOG_LVL_DEBUG
#define LOG_FORMAT LOG_FMT_VERBOSE
#include "log.h"

#define CRTC_REGISTER_COUNT 18
static uint8_t regs[CRTC_REGISTER_COUNT];
#define REG_HORIZONTAL_TOT_CHAR            0
#define REG_HORIZONTAL_DISPLAY_CHAR        1
#define REG_HORIZONTAL_SYNC_PULSE_POSITION 2
#define REG_HORIZONTAL_SYNC_PULSE_WIDTH    3
#define REG_VERTICAL_TOT_CHAR              4
#define REG_TOTAL_RASTER_ADJUST            5
#define REG_VERTICAL_DISPLAY_CHAR          6
#define REG_VERTICAL_SYNC_PULSE_POSITION   7
#define REG_INTERLACED_MODE                8
#define REG_MAX_RASTER_RASTER              9
#define REG_CURSOR_START_RASTER            10
#define REG_CURSOR_END_RASTER              11
#define REG_START_ADDRESS_H                12
#define REG_START_ADDRESS_L                13
#define REG_CURSOR_H                       14
#define REG_CURSOR_L                       15
#define REG_LIGHT_PEN_H                    16
#define REG_LIGHT_PEN_L                    17
unsigned int rselect = 0; // current register selected

#define CRTC_NOT_IMPLEMENTED_STR "not implemented\n"

void crtc_init(void) {
    // TODO
}

uint8_t crtc_in(ceda_ioaddr_t address) {
    (void)address;

    LOG_DEBUG("in: %02x\n", address);

    return 0;
}

void crtc_out(ceda_ioaddr_t address, uint8_t value) {
    LOG_DEBUG("out: [%02x] <= %02x\n", address, value);

    if (address == 0) {
        if (value >= CRTC_REGISTER_COUNT)
            return;
        rselect = value;
        return;
    }

    if (address == 1) {
        // clamp value based on actual number of meaningful bits in each
        // register, and also raise warnings when using non-standard and
        // non-implemented values (emulator specific)
        if (rselect == REG_HORIZONTAL_DISPLAY_CHAR) {
            if (value != 50) {
                LOG_WARN(CRTC_NOT_IMPLEMENTED_STR);
            }
        }
        if (rselect == REG_HORIZONTAL_SYNC_PULSE_WIDTH) {
            value &= 0x0f;
        }
        if (rselect == REG_VERTICAL_TOT_CHAR) {
            value &= 0x7f;
        }
        if (rselect == REG_TOTAL_RASTER_ADJUST) {
            value &= 0x1f;
        }
        if (rselect == REG_VERTICAL_DISPLAY_CHAR) {
            value &= 0x7f;
            if (value != 25) {
                LOG_WARN(CRTC_NOT_IMPLEMENTED_STR);
            }
        }
        if (rselect == REG_VERTICAL_SYNC_PULSE_POSITION) {
            value &= 0x7f;
        }
        if (rselect == REG_INTERLACED_MODE) {
            value &= 0x03;
            if (value != 0 && value != 2)
                LOG_WARN(CRTC_NOT_IMPLEMENTED_STR);
        }
        if (rselect == REG_MAX_RASTER_RASTER) {
            value &= 0x1f;
        }
        if (rselect == REG_CURSOR_START_RASTER) {
            value &= 0x7f;
        }
        if (rselect == REG_CURSOR_END_RASTER) {
            value &= 0x1f;
        }
        if (rselect == REG_START_ADDRESS_H) {
            value &= 0x3f;
        }
        if (rselect == REG_CURSOR_H) {
            value &= 0x3f;
        }
        if (rselect == REG_LIGHT_PEN_H || rselect == REG_LIGHT_PEN_L) {
            // light pen registers are read-only
            return;
        }

        regs[rselect] = value;

        LOG_DEBUG("cursor = %u\n",
                  regs[REG_CURSOR_H] * 256U + regs[REG_CURSOR_L]);

        return;
    }

    assert(0);
}

CRTCCursorBlink crtc_cursorBlink(void) {
    if (!(regs[REG_CURSOR_START_RASTER] & 0x40))
        return CRTC_CURSOR_SOLID;

    return (regs[REG_CURSOR_START_RASTER] & 0x20) ? CRTC_CURSOR_BLINK_FAST
                                                  : CRTC_CURSOR_BLINK_SLOW;
}

unsigned int crtc_cursorPosition(void) {
    return regs[REG_CURSOR_H] * 256U + regs[REG_CURSOR_L];
}

void crtc_cursorRasterSize(uint8_t *start, uint8_t *end) {
    *start = regs[REG_CURSOR_START_RASTER] & 0x1f;
    *end = regs[REG_CURSOR_END_RASTER] & 0x1f;
}

uint16_t crtc_startAddress(void) {
    const uint16_t start_address =
        (regs[REG_START_ADDRESS_H] << 8) | (regs[REG_START_ADDRESS_L]);

    return start_address;
}
