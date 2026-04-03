#include "../ceda_string.h"
#include "../conf.h"
#include "../log.h"

#include <ini.h>

#include <stdint.h>

#include "../hexdump.h"
#include <criterion/criterion.h>

Test(conf, load_bool) {
    static bool values[8] = {
        1, 0, 1, 0, -7, 7, 123, 0,
    };
    static bool expected[8] = {
        false, true, false, true, -64, 63, 123, 0,
    };
    static conf_tuple_t conf[] = {
        {"test", "key0", CONF_BOOL, &values[0]},
        {"test", "key1", CONF_BOOL, &values[1]},
        {"test", "key2", CONF_BOOL, &values[2]},
        {"test", "key3", CONF_BOOL, &values[3]},
        {"test", "key4", CONF_BOOL, &values[4]},
        {"test", "key5", CONF_BOOL, &values[5]},
        {"test", "key6", CONF_BOOL, &values[6]},
        {"test", "key7", CONF_BOOL, &values[7]},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/bool.ini", conf_testGetHandler(), conf),
                  0);

    LOG_DEBUG("actual =\n");
    hexdump(values, sizeof(values));
    LOG_DEBUG("expected =\n");
    hexdump(expected, sizeof(expected));

    cr_assert_arr_eq(values, expected, sizeof(values));
}

Test(conf, load_u32) {
    static uint32_t values[4] = {0};
    static uint32_t expected[4] = {
        0U,
        4294967295U,
        67489U,
        3847982655U,
    };
    static conf_tuple_t conf[] = {
        {"test", "key0", CONF_U32, &values[0]},
        {"test", "key1", CONF_U32, &values[1]},
        {"test", "key2", CONF_U32, &values[2]},
        {"test", "key3", CONF_U32, &values[3]},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/u32.ini", conf_testGetHandler(), conf),
                  0);

    cr_assert_arr_eq(values, expected, sizeof(values));
}

Test(conf, load_mix) {
    struct conf_mix_t {
        bool value0;
        uint32_t value1;
        bool value2;
        bool value3;
        uint32_t value4;
    };
    static struct conf_mix_t values;
    static struct conf_mix_t expected = {
        .value0 = true,
        .value1 = 1234,
        .value2 = false,
        .value3 = true,
        .value4 = 85726,
    };
    static conf_tuple_t conf[] = {
        {"test", "key0", CONF_BOOL, &values.value0},
        {"test", "key1", CONF_U32, &values.value1},
        {"test", "key2", CONF_BOOL, &values.value2},
        {"test", "key3", CONF_BOOL, &values.value3},
        {"test", "key4", CONF_U32, &values.value4},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/mix.ini", conf_testGetHandler(), conf),
                  0);

    cr_assert_eq(values.value0, expected.value0);
    cr_assert_eq(values.value1, expected.value1);
    cr_assert_eq(values.value2, expected.value2);
    cr_assert_eq(values.value3, expected.value3);
    cr_assert_eq(values.value4, expected.value4);
}

Test(conf, get_string) {
    static ceda_string_t *value = NULL;
    ceda_string_t *expected = ceda_string_new(0);
    ceda_string_cpy(expected, "the quick brown fox jumped over the lazy dog");
    static conf_tuple_t conf[] = {
        {"test", "key0", CONF_STR, (void *)&value},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/str.ini", conf_testGetHandler(), conf),
                  0);

    cr_assert_not_null(value);
    cr_assert(ceda_string_eq(value, expected));

    ceda_string_delete(value);
    ceda_string_delete(expected);
}

Test(conf, get_mix) {
    struct conf_mix_t {
        bool value0;
        uint32_t value1;
        // skip value2 for test
        ceda_string_t *value3;
    };
    static struct conf_mix_t values;
    static conf_tuple_t conf[] = {
        {"test", "key0", CONF_BOOL, &values.value0},
        {"test", "key1", CONF_U32, &values.value1},
        // skip key2/value2 for test
        {"test", "key3", CONF_STR, (void *)&values.value3},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(
        ini_parse("test/conf/get_mix.ini", conf_testGetHandler(), conf), 0);

    conf_getType_t getType = conf_testGetGetType();
    bool *value0 = getType(conf, "test", "key0", CONF_BOOL);
    uint32_t *value1 = getType(conf, "test", "key1", CONF_U32);
    uint32_t *value2 = getType(conf, "test", "key2", CONF_U32);
    ceda_string_t **value3 =
        (ceda_string_t **)getType(conf, "test", "key3", CONF_STR);

    cr_assert_not_null(value0);
    cr_assert_eq(*value0, true);

    cr_assert_not_null(value1);
    cr_assert_eq(*value1, 42);

    cr_assert_null(value2);

    ceda_string_t *expected_value3 = ceda_string_new(0);
    ceda_string_cpy(expected_value3, "a nice emulator");

    cr_assert_not_null(*value3);
    cr_assert(ceda_string_eq(*value3, expected_value3));
    ceda_string_delete(expected_value3);
}

Test(conf, overwrite) {
    static struct {
        bool value0;
        uint32_t value1;
        ceda_string_t *value2;
    } values;
    static conf_tuple_t conf[] = {
        {"test", "key0", CONF_BOOL, &values.value0},
        {"test", "key1", CONF_U32, &values.value1},
        {"test", "key2", CONF_STR, (void *)&values.value2},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(
        ini_parse("test/conf/overwrite.ini", conf_testGetHandler(), conf), 0);

    cr_assert_eq(values.value0, false);
    cr_assert_eq(values.value1, 2712);
    ceda_string_t *expected_string = ceda_string_new(0);
    ceda_string_cpy(expected_string, "hello new world");
    cr_assert(ceda_string_eq(values.value2, expected_string));
    ceda_string_delete(expected_string);
}
