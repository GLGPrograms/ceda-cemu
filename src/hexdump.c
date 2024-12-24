#include "hexdump.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

#define BUFFER_SIZE (1024)

/**
 * @brief Print a nice hexdump.
 *
 * @param buffer Pointer to binary data.
 * @param size Length of binary data.
 */
void hexdump(const void *buffer, size_t size) {
    const uint8_t *blob = (const uint8_t *)buffer;
    char output[BUFFER_SIZE] = {0};
    int n = 0;
    char ascii[16 + 1] = {0};
    const uint16_t address = 0;

    for (unsigned int i = 0; i < size && n < BUFFER_SIZE - 1; ++i) {
        const char c = (char)blob[i];

        if (i % 16 == 0) {
            n += snprintf(output + n, (size_t)(BUFFER_SIZE - n), "%04x\t",
                          address + i);
        }

        n += snprintf(output + n, (size_t)(BUFFER_SIZE - n), "%02x ",
                      ((unsigned int)(c)) & 0xff);
        ascii[i % 16] = isprint(c) ? c : '.';

        if (i % 16 == 7) {
            n += snprintf(output + n, (size_t)(BUFFER_SIZE - n), " ");
        }

        if (i % 16 == 15) {
            n += snprintf(output + n, (size_t)(BUFFER_SIZE - n), "\t%s\n",
                          ascii);
        }
    }

    LOG_DEBUG("%s\n", output);
}
