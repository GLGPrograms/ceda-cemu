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

/**
 * @brief Read a sector from a certain drive
 *
 * @param buffer pointer to byte buffer where to load the sector. Buffer may be
 *               NULL to only fetch sector size.
 * @param unit_number drive number where to unload the image
 * @param phy_head physical head, from which disk side will the disk be read
 * @param phy_track physical track, where the track is currently placed on the
 *                  drive
 * @param head logical head, must match with sector descriptor
 * @param track logical track, must match with sector descriptor
 * @param sector logical sector, which sector has to be read
 * @return is 0 when successful, -1 for any kind of error (no image loaded,
 *         invalid parameters, ...)
 */
ssize_t floppy_read_buffer(uint8_t *buffer, uint8_t unit_number, bool phy_head,
                           uint8_t phy_track, bool head, uint8_t track,
                           uint8_t sector);

#endif