#ifndef CEDA_VIDEO_H
#define CEDA_VIDEO_H

#include "module.h"

#include <Z80.h>
#include <stdbool.h>

void video_init(CEDAModule *mod);

zuint8 video_ram_read(void *context, zuint16 address);
void video_ram_write(void *context, zuint16 address, zuint8 value);
void video_bank(bool attr);

#endif // CEDA_VIDEO_H
