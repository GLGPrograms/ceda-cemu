#ifndef CEDA_CRTC_H
#define CEDA_CRTC_H

#include <Z80.h>

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

#endif // CEDA_CRTC_H
