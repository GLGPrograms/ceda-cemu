#include "charmon.h"

#include "conf.h"
#include "ubus.h"

#include <stdio.h>
#include <string.h>

#define CHARMON_BASE (0xF0)

void charmon_out(ceda_ioaddr_t address, uint8_t value) {
    (void)address;
    (void)putc(value, stdout);
}

void charmon_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));

    bool *conf_installed = conf_getBool("mod", "charmon_installed");
    bool installed = conf_installed ? *conf_installed : false;

    if (!installed)
        return;

    ubus_register(CHARMON_BASE, CHARMON_BASE + 1, NULL, charmon_out);
}
