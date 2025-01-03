#ifndef CEDA_SIO2_H
#define CEDA_SIO2_H

#include "module.h"
#include "type.h"

#include <stdbool.h>
#include <stdint.h>

#include <Z80.h>

typedef enum sio_channel_idx_t {
    SIO_CHANNEL_A = 0,
    SIO_CHANNEL_B = 1,

    SIO_CHANNEL_CNT,
} sio_channel_idx_t;

typedef bool (*sio_channel_try_read_t)(uint8_t *c);
typedef bool (*sio_channel_try_write_t)(uint8_t c);

void sio2_init(CEDAModule *mod);
uint8_t sio2_in(ceda_ioaddr_t address);
void sio2_out(ceda_ioaddr_t address, uint8_t value);

#endif // CEDA_SIO2_H
