#include "floppy.h"

#include <stdio.h>
#include <stdlib.h>

#include "macro.h"

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
    FILE *fd = fopen(filename, "rb");

    if (fd == NULL)
        return -1;

    floppy_units[unit_number].fd = fd;

    return 0;
}

ssize_t floppy_unload_image(unsigned int unit_number) {
    FILE *fd = floppy_units[unit_number].fd;

    if (fd == NULL)
        return -1;

    floppy_units[unit_number].fd = NULL;

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
ssize_t floppy_read_buffer(uint8_t *buffer, uint8_t unit_number, bool phy_head,
                           uint8_t phy_track, bool head, uint8_t track,
                           uint8_t sector) {
    assert(unit_number < ARRAY_SIZE(floppy_units));

    FILE *fd = floppy_units[unit_number].fd;
    size_t len = 256;
    long offset;

    // No disk loaded, raise error
    if (fd == NULL)
        return -1;

    // TODO(giuliof): at the moment we support only drive 0
    if (unit_number != 0)
        return -1;

    // Due to its structure, with Ceda File Format the physical head and track
    // must be same as their logical counterpart.
    if (phy_head != head)
        return -1;
    if (phy_track != track)
        return -1;

    // CFF has up to 80 tracks
    if (track > 79)
        return -1;

    // Locate sector start
    if (track == 0 && head == 0) {
        // First track has max 16 sectors
        if (sector > 15)
            return -1;
        // Compute byte offset to sector start
        offset = (long)sector * 256;
    } else {
        if (sector > 5)
            return -1;

        // Compute byte offset to sector start
        offset = (long)track * 1024 * 5 * 2;
        offset += (long)head * 1024 * 5;
        offset += (long)sector * 1024;

        // First track has a different format
        offset -= (long)1024 * 5;
        offset += (long)256 * 16;

        len = 1024;
    }

    // If requested, load sector into buffer
    if (buffer) {
        if (fseek(fd, offset, SEEK_SET) < 0)
            return -1;

        len = fread(buffer, sizeof(uint8_t), len, fd);
    }

    return (int)len;
}
