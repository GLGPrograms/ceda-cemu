#ifndef CEDA_FDC_H
#define CEDA_FDC_H

#include "type.h"

#include <Z80.h>
#include <stdbool.h>

typedef enum disk_image_err_t {
    DISK_IMAGE_NOMEDIUM = 0, // No medium available
    DISK_IMAGE_ERR = -1,     // Generic error
    DISK_IMAGE_INVALID_GEOMETRY = -2,
} disk_image_err_t;

typedef int (*fdc_read_write_t)(uint8_t *buffer, uint8_t unit_number,
                                bool phy_head, uint8_t phy_track, bool head,
                                uint8_t track, uint8_t sector);

void fdc_init(void);

uint8_t fdc_in(ceda_ioaddr_t address);
void fdc_out(ceda_ioaddr_t address, uint8_t value);
void fdc_tc_out(ceda_ioaddr_t address, uint8_t value);
bool fdc_getIntStatus(void);
void fdc_kickDiskImage(fdc_read_write_t read_callback,
                       fdc_read_write_t write_callback);

#endif // CEDA_FDC_H
