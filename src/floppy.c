#include "floppy.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fdc.h"
#include "macro.h"

#define CFF_MAXIMUM_TRACKS (80U)
#define CFF_SECTOR_SIZE    (1024U)
#define CFF_MAX_SECTORS    (5U)
#define CFF_T0_SECTOR_SIZE (256U)
#define CFF_T0_MAX_SECTORS (16U)

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
static int floppy_read_buffer(uint8_t *buffer, uint8_t unit_number,
                              bool phy_head, uint8_t phy_track, bool head,
                              uint8_t track, uint8_t sector);

static int floppy_write_buffer(uint8_t *buffer, uint8_t unit_number,
                               bool phy_head, uint8_t phy_track, bool head,
                               uint8_t track, uint8_t sector);

// TODO(giuliof): this structure will contain the type of image loaded.
// At the moment, only Ceda File Format is supported (linearized binary disk
// image)
typedef struct floppy_unit_t {
    FILE *fd;
} floppy_unit_t;

floppy_unit_t floppy_units[4];

ssize_t floppy_load_image(const char *filename, unsigned int unit_number) {
    assert(unit_number < ARRAY_SIZE(floppy_units));

    // Just unload previously loaded images
    floppy_unload_image(unit_number);

    // TODO(giuliof): if extension is ..., then image format is ...
    FILE *fd = fopen(filename, "rb+");

    if (fd == NULL)
        return -1;

    floppy_units[unit_number].fd = fd;

    fdc_kickDiskImage(floppy_read_buffer, floppy_write_buffer);

    return 0;
}

ssize_t floppy_unload_image(unsigned int unit_number) {
    FILE *fd = floppy_units[unit_number].fd;

    if (fd == NULL)
        return -1;

    floppy_units[unit_number].fd = NULL;

    fdc_kickDiskImage(NULL, NULL);

    if (fclose(fd) < 0)
        return -1;

    return 0;
}

/*
 * A sector is read from Ceda File Format image.
 * This image is a linearized binary dump of the floppy, ordered by sector,
 * then head, then track.
 * Ceda/Sanco disks have 80 tracks, 2 sides and are formatted, by default, with
 * first track with 256 bytes per sector and 16 sectors per track, others with
 * 1024 bps and 5 spt. The Ceda File Format reflects this formatting layout.
 */
static int floppy_read_buffer(uint8_t *buffer, uint8_t unit_number,
                              bool phy_head, uint8_t phy_track, bool head,
                              uint8_t track, uint8_t sector) {
    if (unit_number >= ARRAY_SIZE(floppy_units))
        return DISK_IMAGE_NOMEDIUM;

    FILE *fd = floppy_units[unit_number].fd;
    size_t len = CFF_T0_SECTOR_SIZE;
    uint32_t offset;

    // No disk loaded
    if (fd == NULL)
        return DISK_IMAGE_NOMEDIUM;

    // Due to its structure, with Ceda File Format the physical head and track
    // must be same as their logical counterpart.
    if (phy_head != head)
        return DISK_IMAGE_INVALID_GEOMETRY;
    if (phy_track != track)
        return DISK_IMAGE_INVALID_GEOMETRY;

    // CFF has up to 80 tracks
    if (track > CFF_MAXIMUM_TRACKS - 1)
        return DISK_IMAGE_INVALID_GEOMETRY;

    // Locate sector start
    if (track == 0 && head == 0) {
        // First track different format handling
        if (sector > CFF_T0_MAX_SECTORS - 1)
            return DISK_IMAGE_INVALID_GEOMETRY;
        // Compute byte offset to sector start
        offset = sector * CFF_T0_SECTOR_SIZE;
    } else {
        if (sector > CFF_MAX_SECTORS)
            return DISK_IMAGE_INVALID_GEOMETRY;

        // Compute byte offset to sector start
        offset = track * CFF_SECTOR_SIZE * CFF_MAX_SECTORS * 2;
        offset += head * CFF_SECTOR_SIZE * CFF_MAX_SECTORS;
        offset += sector * CFF_SECTOR_SIZE;

        // First track has a different format
        offset -= CFF_SECTOR_SIZE * CFF_MAX_SECTORS;
        offset += CFF_T0_SECTOR_SIZE * CFF_T0_MAX_SECTORS;

        len = CFF_SECTOR_SIZE;
    }

    // If requested, load sector into buffer
    if (buffer) {
        if (fseek(fd, offset, SEEK_SET) < 0)
            return DISK_IMAGE_ERR;

        len = fread(buffer, sizeof(uint8_t), len, fd);
    }

    return (int)len;
}

static int floppy_write_buffer(uint8_t *buffer, uint8_t unit_number,
                               bool phy_head, uint8_t phy_track, bool head,
                               uint8_t track, uint8_t sector) {
    if (unit_number >= ARRAY_SIZE(floppy_units))
        return DISK_IMAGE_NOMEDIUM;

    FILE *fd = floppy_units[unit_number].fd;
    size_t len = CFF_T0_SECTOR_SIZE;
    uint32_t offset;

    // No disk loaded, raise error
    if (fd == NULL)
        return DISK_IMAGE_NOMEDIUM;

    // Due to its structure, with Ceda File Format the physical head and track
    // must be same as their logical counterpart.
    if (phy_head != head)
        return DISK_IMAGE_INVALID_GEOMETRY;
    if (phy_track != track)
        return DISK_IMAGE_INVALID_GEOMETRY;

    // CFF has up to 80 tracks
    if (track > CFF_MAXIMUM_TRACKS - 1)
        return DISK_IMAGE_INVALID_GEOMETRY;

    // Locate sector start
    if (track == 0 && head == 0) {
        // First track different format handling
        if (sector > CFF_T0_MAX_SECTORS - 1)
            return DISK_IMAGE_INVALID_GEOMETRY;
        // Compute byte offset to sector start
        offset = sector * CFF_T0_SECTOR_SIZE;
    } else {
        if (sector > CFF_MAX_SECTORS)
            return DISK_IMAGE_INVALID_GEOMETRY;

        // Compute byte offset to sector start
        offset = track * CFF_SECTOR_SIZE * CFF_MAX_SECTORS * 2;
        offset += head * CFF_SECTOR_SIZE * CFF_MAX_SECTORS;
        offset += sector * CFF_SECTOR_SIZE;

        // First track has a different format
        offset -= CFF_SECTOR_SIZE * CFF_MAX_SECTORS;
        offset += CFF_T0_SECTOR_SIZE * CFF_T0_MAX_SECTORS;

        len = CFF_SECTOR_SIZE;
    }

    // If requested, load sector into buffer
    if (buffer) {
        if (fseek(fd, offset, SEEK_SET) < 0)
            return DISK_IMAGE_ERR;

        len = fwrite(buffer, sizeof(uint8_t), len, fd);
    }

    return (int)len;
}
