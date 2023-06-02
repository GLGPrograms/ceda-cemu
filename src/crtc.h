#ifndef CEDA_CRTC_H
#define CEDA_CRTC_H

#include <Z80.h>
#include <stdint.h>

typedef enum CRTCCursorBlink {
    CRTC_CURSOR_SOLID,
    CRTC_CURSOR_BLINK_SLOW,
    CRTC_CURSOR_BLINK_FAST,
} CRTCCursorBlink;

void crtc_init(void);

zuint8 crtc_in(void *context, zuint16 address);
void crtc_out(void *context, zuint16 address, zuint8 value);

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

#endif // CEDA_CRTC_H
