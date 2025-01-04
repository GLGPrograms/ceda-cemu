#ifndef CEDA_FLOPPY_H
#define CEDA_FLOPPY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// TODO(giuliof): the floppy interface is not well defined, especially regarding
// error condition returns. See "Handling FDC errors" issue on github.

/**
 * @brief Loads floppy image by filename
 *
 * @param filename string with relative or full image file path
 * @param unit_number drive number where to load the image
 * @return is 0 when successful, -1 if file does not exists
 */
ssize_t floppy_load_image(const char *filename, unsigned int unit_number);

/**
 * @brief Unload floppy image from a certain drive
 *
 * @param unit_number drive number where to unload the image
 * @return is 0 when successful, -1 if no image was already loaded
 */
ssize_t floppy_unload_image(unsigned int unit_number);

#endif
