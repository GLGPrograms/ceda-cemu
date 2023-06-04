#ifndef CEDA_VIDEO_H
#define CEDA_VIDEO_H

#include <Z80.h>
#include <stdbool.h>

void video_init(void);
void video_start(void);
void video_update(void);
long video_remaining(void);
void video_cleanup(void);

zuint8 video_ram_read(void *context, zuint16 address);
void video_ram_write(void *context, zuint16 address, zuint8 value);
void video_bank(bool attr);

#endif // CEDA_VIDEO_H
