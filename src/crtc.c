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
#define REG_CURSOR_H                       14
#define REG_CURSOR_L                       15
#define REG_LIGHT_PEN_H                    16
#define REG_LIGHT_PEN_L                    17
unsigned int rselect = 0; // current register selected

#define CRTC_NOT_IMPLEMENTED_STR "not implemented\n"

void crtc_init(void) {
    // TODO
}

zuint8 crtc_in(void *context, zuint16 address) {
    (void)context;
    (void)address;

    LOG_DEBUG("in: %02x\n", address);

    return 0;
}

void crtc_out(void *context, zuint16 address, zuint8 value) {
    (void)context;

    LOG_DEBUG("out: [%02x] <= %02x\n", address, value);

    if (address == 0) {
        if (value >= CRTC_REGISTER_COUNT)
            return;
        rselect = value;
        return;
    }

    if (address == 1) {
        regs[rselect] = value;

        if (rselect == REG_HORIZONTAL_DISPLAY_CHAR && value != 50)
            LOG_WARN(CRTC_NOT_IMPLEMENTED_STR);
        if (rselect == REG_VERTICAL_DISPLAY_CHAR && value != 25)
            LOG_WARN(CRTC_NOT_IMPLEMENTED_STR);
        if (rselect == REG_INTERLACED_MODE)
            if (value != 0 && value != 2)
                LOG_WARN(CRTC_NOT_IMPLEMENTED_STR);

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
