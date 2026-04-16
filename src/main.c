#include "ceda.h"

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

int main(int argc, char *argv[]) {
    int ret = 0;

    LOG_INFO("CEDA Emulator\n");
    LOG_INFO("prefix = %s\n", CEDA_PREFIX);

    (void)argc;
    (void)argv;
    ceda_init();
    ret = ceda_run();

    return ret;
}
