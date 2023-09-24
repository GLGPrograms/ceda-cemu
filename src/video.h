#ifndef CEDA_VIDEO_H
#define CEDA_VIDEO_H

#include "module.h"
#include "type.h"

#include <Z80.h>
#include <stdbool.h>

void video_init(CEDAModule *mod);

uint8_t video_ram_read(ceda_address_t address);
void video_ram_write(ceda_address_t address, uint8_t value);
void video_bank(bool attr);

void video_frameSyncReset(void);
bool video_frameSync(void);

#endif // CEDA_VIDEO_H
