#include "time.h"

#include <stddef.h>
#include <sys/time.h>

ms_time_t time_now_ms(void) {
    return time_now_us() / 1000;
}

us_time_t time_now_us(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    const us_time_t now_us = now.tv_sec * 1000 * 1000 + now.tv_usec;
    return now_us;
}
