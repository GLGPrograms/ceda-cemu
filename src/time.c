#include "time.h"

#include <stddef.h>
#include <sys/time.h>

long time_now_ms(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    const long now_ms = now.tv_sec * 1000 + now.tv_usec / 1000;
    return now_ms;
}
