#include <criterion/criterion.h>

#include <stddef.h>
#include <stdint.h>

#include "../monitor.h"

Test(monitor, empty) {
    const Monitor *monitors;
    size_t n = monitor_get(&monitors);
    cr_assert_neq(n, 0);

    for (size_t i = 0; i < n; ++i) {
        cr_expect_eq(monitors[i].valid, false);
    }
}

Test(monitor, deleteNonExistent) {
    bool ok = monitor_delete(0);
    cr_assert_eq(ok, false);
}

Test(monitor, exec_breakpoints) {
    const uint16_t ADDRESS = 0x1234;
    const uint16_t NOTADDRESS = 0x5678;

    bool ok = monitor_addBreakpoint(ADDRESS);
    cr_assert_eq(ok, true);

    const Monitor *monitors;
    monitor_get(&monitors);
    cr_assert_eq(monitors[0].valid, true);
    cr_assert_eq(monitors[0].address, ADDRESS);

    bool hit = false;

    // executing an address which does not have a breakpoint set
    hit = monitor_checkBreakpoint(NOTADDRESS);
    cr_assert_eq(hit, false);

    // hitting breakpoint for the first time
    hit = monitor_checkBreakpoint(ADDRESS);
    cr_assert_eq(hit, true);

    // hitting breakpoint for the second time
    // breakpoint must be disabled otherwise CPU can't continue
    hit = monitor_checkBreakpoint(ADDRESS);
    cr_assert_eq(hit, false);

    // hitting breakpoint for the third time
    // breakpoint must be restored
    hit = monitor_checkBreakpoint(ADDRESS);
    cr_assert_eq(hit, true);
}

Test(monitor, read_watchpoint) {
    const uint16_t ADDRESS = 0x1234;
    const uint16_t NOTADDRESS = 0x5678;

    bool ok = monitor_addReadWatchpoint(ADDRESS);
    cr_assert_eq(ok, true);

    bool hit = false;
    hit = monitor_checkReadWatchpoint(NOTADDRESS);
    cr_assert_eq(hit, false);

    hit = monitor_checkReadWatchpoint(ADDRESS);
    cr_assert_eq(hit, true);

    hit = monitor_checkWriteWatchpoint(ADDRESS, 0x00);
    cr_assert_eq(hit, false);
}

Test(monitor, write_watchpoint) {
    const uint16_t ADDRESS_WITH_VAL = 0xabcd;
    const uint16_t ADDRESS_NO_VAL = 0xef01;
    const uint8_t VALUE = 0x42;
    const uint8_t NOTVALUE = 0x77;

    bool ok = false;
    bool hit = false;

    ok = monitor_addWriteWatchpoint(ADDRESS_WITH_VAL, &VALUE);
    cr_assert_eq(ok, true);
    ok = monitor_addWriteWatchpoint(ADDRESS_NO_VAL, NULL);
    cr_assert_eq(ok, true);

    hit = monitor_checkWriteWatchpoint(ADDRESS_WITH_VAL, NOTVALUE);
    cr_assert_eq(hit, false);
    hit = monitor_checkWriteWatchpoint(ADDRESS_WITH_VAL, VALUE);
    cr_assert_eq(hit, true);
    hit = monitor_checkWriteWatchpoint(ADDRESS_NO_VAL, 0x00);
    cr_assert_eq(hit, true);
}

Test(monitor, deleteBreakpoint) {
    const Monitor *monitors = NULL;
    size_t n = 0;
    size_t valid_cnt = 0;

    const uint16_t ADDRESS = 0x1234;
    monitor_addBreakpoint(ADDRESS);

    n = monitor_get(&monitors);
    valid_cnt = 0;
    for (size_t i = 0; i < n; ++i)
        if (monitors[i].valid)
            valid_cnt += 1;
    cr_assert_eq(valid_cnt, 1);

    bool ok = monitor_delete(0);
    cr_assert_eq(ok, true);

    n = monitor_get(&monitors);
    valid_cnt = 0;
    for (size_t i = 0; i < n; ++i)
        if (monitors[i].valid)
            valid_cnt += 1;
    cr_assert_eq(valid_cnt, 0);
}
