#include "ceda.h"

#ifdef CEDA_TEST
#include <criterion/criterion.h>
#endif

#include <stdio.h>

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

int main(int argc, char *argv[]) {
    int ret = 0;

#ifdef CEDA_TEST
    LOG_INFO("CEDA Test\n");
    criterion_options.color = true;
    criterion_options.full_stats = true;
    criterion_options.timeout = 1;

    struct criterion_test_set *set = criterion_initialize();
    if (criterion_handle_args(argc, argv, true))
        ret = !criterion_run_all_tests(set);
    criterion_finalize(set);

#else
    LOG_INFO("CEDA Emulator\n");

    (void)argc;
    (void)argv;
    ceda_init();
    ceda_run();
#endif

    return ret;
}
