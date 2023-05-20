#include "speaker.h"

zuint8 speaker_in(void *context, zuint16 address) {
    (void)context;
    (void)address;

    speaker_trigger();

    return 0;
}

void speaker_out(void *context, zuint16 address, zuint8 value) {
    (void)context;
    (void)address;
    (void)value;

    speaker_trigger();
}

void speaker_trigger(void) {
    // TODO -- make "beep"
}