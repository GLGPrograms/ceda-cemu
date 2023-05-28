#include "ceda.h"

#ifdef CEDA_TEST
#include <criterion/criterion.h>
#endif

#include <stdio.h>

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

int main() {
    LOG_INFO("CEDA\n");

    int ret = 0;

#ifdef CEDA_TEST
    criterion_options.color = true;
    criterion_options.full_stats = true;

    struct criterion_test_set *set = criterion_initialize();
    ret = !criterion_run_all_tests(set);
    criterion_finalize(set);

#else
    ceda_init();
    ceda_run();
#endif

    return ret;
}
