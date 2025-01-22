#ifndef CEDA_SERIAL_PORT_H
#define CEDA_SERIAL_PORT_H

#include "module.h"

#include <stdbool.h>
#include <stdint.h>

void serial_init(CEDAModule *mod);

bool serial_open(uint16_t port);
void serial_close(void);

#endif // CEDA_SERIAL_PORT_H
