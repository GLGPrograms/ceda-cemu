#ifndef CEDA_FDC_H
#define CEDA_FDC_H

#include "type.h"

#include <Z80.h>
#include <stdbool.h>

/**
 * @brief Errors acceptable by the Floppy Disk Controller logic and that can be
 * raised by the fdc r/w callbacks
 */
typedef enum disk_image_err_t {
    DISK_IMAGE_NOMEDIUM = 0, // No medium available
    DISK_IMAGE_ERR = -1,     // Generic error
    DISK_IMAGE_INVALID_GEOMETRY = -2,
} disk_image_err_t;

/**
 * @brief Signature of the r/w callbacks used by the Floppy Disk Controller.
 * At the moment, there are only a callback for the reading of data from the
 * virtual medium, and callback to write onto it.
 * Other features, like compare, ID read, ... are not yet supported.
 */
typedef int (*fdc_read_write_t)(uint8_t *buffer, uint8_t unit_number,
                                bool phy_head, uint8_t phy_track, bool head,
                                uint8_t track, uint8_t sector);

/**
 * @brief Initialize the Floppy Disk Controller system
 *
 */
void fdc_init(void);

/**
 * @brief Read data from the Floppy Disk Controller using its bus
 *
 * @param address address inside its memory space (1 bit)
 * @return read value exposed on the data bus
 */
uint8_t fdc_in(ceda_ioaddr_t address);

/**
 * @brief Write data into the Floppy Disk Controller using its bus
 *
 * @param address address inside its memory space (1 bit)
 * @return value to be written into
 */
void fdc_out(ceda_ioaddr_t address, uint8_t value);

/**
 * @brief Assert/deassert the Terminal Count signal of the Floppy Disk
 * Controller
 *
 * @param address dummy value to be compliant to the Z80 peripheral bus
 * @param value the status of the TC signal (boolean, but uint8_t to be
 * compliant with the Z80 peripheral bus)
 */
void fdc_tc_out(ceda_ioaddr_t address, uint8_t value);

/**
 * @brief Get the status of the INT signal of the Floppy Disk Controller
 */
bool fdc_getIntStatus(void);

/**
 * @brief Register the read and write callbacks to the Floppy Disk Controller.
 * This happens when a disk image is virtually inserted.
 * Both arguments can be NULL to eject the image.
 *
 * @param read_callback
 * @param write_callback
 */
void fdc_kickDiskImage(fdc_read_write_t read_callback,
                       fdc_read_write_t write_callback);

#endif // CEDA_FDC_H
