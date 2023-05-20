#include "ceda.h"

#include <stdio.h>

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

int main() {
    LOG_INFO("CEDA\n");

    ceda_init();
    ceda_run();
}
