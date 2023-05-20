#include "speaker.h"

zuint8 speaker_in(void *context, zuint16 address) {
    speaker_trigger();
}

void speaker_out(void *context, zuint16 address, zuint8 value) {
    speaker_trigger();
}

void speaker_trigger(void) {
    // TODO -- make "beep"
}