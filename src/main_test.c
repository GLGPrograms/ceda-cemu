#include <criterion/criterion.h>

#include <stdio.h>

int main(int argc, char *argv[]) {
    int ret = 1;

    printf("CEDA Test\n");
    criterion_options.color = true;
    criterion_options.full_stats = true;
    criterion_options.timeout = 1;
    criterion_options.logging_threshold = CRITERION_INFO;
    criterion_options.jobs = 1;

    struct criterion_test_set *set = criterion_initialize();
    if (criterion_handle_args(argc, argv, true))
        ret = !criterion_run_all_tests(set);
    criterion_finalize(set);

    return ret;
}
