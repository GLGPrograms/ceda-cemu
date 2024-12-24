#ifndef CEDA_CONF_H
#define CEDA_CONF_H

#include <stdbool.h>
#include <stdint.h>

void conf_init(void);

uint32_t *conf_getU32(const char *section, const char *key);
bool *conf_getBool(const char *section, const char *key);
const char *conf_getString(const char *section, const char *key);

void conf_cleanup(void);

#endif // CEDA_CONF_H
