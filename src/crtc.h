#ifndef CEDA_CRTC_H
#define CEDA_CRTC_H

#include "type.h"

#include <Z80.h>
#include <stdint.h>

typedef enum CRTCCursorBlink {
    CRTC_CURSOR_SOLID,
    CRTC_CURSOR_BLINK_SLOW,
    CRTC_CURSOR_BLINK_FAST,
} CRTCCursorBlink;

void crtc_init(void);

uint8_t crtc_in(ceda_ioaddr_t address);
void crtc_out(ceda_ioaddr_t address, uint8_t value);

/**
 * @brief Check if the cursor is being blinked by the hardware.
 *
 * @return CRTCCursorBlink cursor blinking status
 */
CRTCCursorBlink crtc_cursorBlink(void);

/**
 * @brief Get current cursor position (linearized).
 *
 * @return int = row * total_columns + column
 */
unsigned int crtc_cursorPosition(void);

/**
 * @brief Get the cursor size in terms of start/end raster line.
 *
 * @param start High raster line (included).
 * @param end Low raster line (included).
 */
void crtc_cursorRasterSize(uint8_t *start, uint8_t *end);

/**
 * @brief Get current video memory start address.
 *
 * Start address is relative to CRCT.
 *
 * @return uint16_t Start address.
 */
uint16_t crtc_startAddress(void);

#endif // CEDA_CRTC_H
