#include "conf.h"

#include "ceda_string.h"
#include "macro.h"
#include "tokenizer.h"

#include <ini.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static const char *CONF_PATH_CWD = "./ceda-cemu.ini";
static const char *CONF_PATH_HOME =
    "/.config/it.glgprograms.retrofficina/ceda-cemu.ini";

#define CONF_WORD_SIZE 8
#define CONF_PATH_SIZE 512

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

// Emulator dynamic configuration
static struct {
    bool cge_installed;
    bool charmon_installed;
    ceda_string_t *bios_rom_path;
    ceda_string_t *char_rom_path;
    ceda_string_t *cge_rom_path;
} conf;

typedef enum conf_type_t {
    CONF_NONE,
    CONF_U32,
    CONF_BOOL,
    CONF_STR,

    CONF_TYPE_CNT,
} conf_type_t;

typedef struct conf_tuple_t {
    const char *section;
    const char *key;
    conf_type_t type;
    void *value;
} conf_tuple_t;

static conf_tuple_t conf_tuples[] = {
    {"mod", "cge_installed", CONF_BOOL, &conf.cge_installed},
    {"mod", "charmon_installed", CONF_BOOL, &conf.charmon_installed},
    {"path", "bios_rom", CONF_STR, &conf.bios_rom_path},
    {"path", "char_rom", CONF_STR, &conf.char_rom_path},
    {"path", "cge_rom", CONF_STR, &conf.cge_rom_path},
    {NULL, NULL, CONF_NONE, NULL},
};

/**
 * @brief Populate the emulator dynamic user configuration.
 *
 * This is the callback for libinih, the INI configuration file parser library.
 * This function is called for every section/key/value tuple found
 * by the library, and must return 1 in case of success, and 0 in case of error.
 * Since we are not interested in stopping the parser in case of invalid
 * configuration (for now), the return value has no actual effect; nevertheless,
 * it must be meaningful, in case we need to use it in the future.
 *
 * @param user User-specific data pointer (emulator configuration struct)
 * @param section INI section
 * @param key INI key
 * @param value INI value, as NUL-terminated C-string
 *
 * @return 1 in case of success, 0 otherwise
 */
static int conf_handler(void *user, const char *section, const char *key,
                        const char *value) {
    LOG_DEBUG("user = %p, section = %s, key = %s, value = %s\n", user, section,
              key, value);

    for (conf_tuple_t *tuple = (conf_tuple_t *)user; tuple->section != NULL;
         ++tuple) {
        if (strcmp(tuple->section, section) != 0)
            continue;
        if (strcmp(tuple->key, key) != 0)
            continue;

        assert(tuple->type < CONF_TYPE_CNT);
        assert(tuple->value);

        switch (tuple->type) {
        case CONF_BOOL: {
            // accept 0/other as valid boolean values
            unsigned int n;
            if (tokenizer_next_int(&n, value)) {
                *((bool *)(tuple->value)) = n;
                return 1;
            }

            // also accept "true" and "false" as valid boolean values
            char word[CONF_WORD_SIZE];
            if (tokenizer_next_word(word, value, CONF_WORD_SIZE)) {
                if (strcmp(word, "true") == 0) {
                    *((bool *)(tuple->value)) = true;
                    return 1;
                }
                if (strcmp(word, "false") == 0) {
                    *((bool *)(tuple->value)) = false;
                    return 1;
                }
            }
            break;
        }
        case CONF_U32: {
            unsigned int n;
            if (tokenizer_next_int(&n, value)) {
                *((uint32_t *)(tuple->value)) = n;
                return 1;
            }
            break;
        }
        case CONF_STR: {
            ceda_string_t **ptr = (ceda_string_t **)tuple->value;

            // overwrite previous string, if any
            if (*ptr != NULL) {
                ceda_string_delete(*ptr);
            }

            *ptr = ceda_string_new((ceda_size_t)(strlen(value) + 1));
            ceda_string_cpy(*ptr, value);
            return 1;
        }
        default:
            LOG_ERR("INI parser fault\n");
            abort();
        }
    }

    LOG_WARN("can not parse INI: section = %s, key = %s, value = %s\n", section,
             key, value);
    return 0; // error
}

void conf_init(void) {
    const char *loaded_path = NULL;
    char path[CONF_PATH_SIZE];

    // load ini from local working directory
    if (ini_parse(CONF_PATH_CWD, conf_handler, conf_tuples) >= 0)
        loaded_path = CONF_PATH_CWD;

    // load ini from user home, if not in local directory
    if (loaded_path == NULL) {
        const char *home = getenv("HOME");
        if (home) {
            (void)snprintf(path, CONF_PATH_SIZE, "%s/%s", home, CONF_PATH_HOME);
            if (ini_parse(path, conf_handler, conf_tuples) >= 0)
                loaded_path = path;
        }
    }

    if (loaded_path)
        LOG_INFO("load INI configuration from: %s\n", loaded_path);
    else
        LOG_WARN("unable to load INI configuration, using default values\n");
}

void conf_cleanup(void) {
    for (conf_tuple_t *tuple = conf_tuples; tuple->section != NULL; ++tuple) {
        if (tuple->type != CONF_STR)
            continue;

        ceda_string_t **ptr = (ceda_string_t **)tuple->value;
        if (*ptr == NULL)
            continue;

        ceda_string_delete(*ptr);
    }
}

static void *conf_getType(conf_tuple_t *tuples, const char *section,
                          const char *key, conf_type_t type) {
    for (const conf_tuple_t *tuple = tuples; tuple->section != NULL; ++tuple) {
        if (strcmp(tuple->section, section) != 0)
            continue;
        if (strcmp(tuple->key, key) != 0)
            continue;

        CEDA_STRONG_ASSERT_TRUE(tuple->type == type);
        CEDA_STRONG_ASSERT_VALID_PTR(tuple->value);

        return tuple->value;
    }

    return NULL;
}

uint32_t *conf_getU32(const char *section, const char *key) {
    return conf_getType(conf_tuples, section, key, CONF_U32);
}

bool *conf_getBool(const char *section, const char *key) {
    return conf_getType(conf_tuples, section, key, CONF_BOOL);
}

const char *conf_getString(const char *section, const char *key) {
    ceda_string_t **string = conf_getType(conf_tuples, section, key, CONF_STR);

    if (string == NULL || *string == NULL)
        return NULL;

    return ceda_string_data(*string);
}

#if defined(CEDA_TEST)

#include "hexdump.h"
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
    cr_assert_geq(ini_parse("test/conf/bool.ini", conf_handler, conf), 0);

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
    cr_assert_geq(ini_parse("test/conf/u32.ini", conf_handler, conf), 0);

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
    cr_assert_geq(ini_parse("test/conf/mix.ini", conf_handler, conf), 0);

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
        {"test", "key0", CONF_STR, &value},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/str.ini", conf_handler, conf), 0);

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
        {"test", "key3", CONF_STR, &values.value3},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/get_mix.ini", conf_handler, conf), 0);

    bool *value0 = conf_getType(conf, "test", "key0", CONF_BOOL);
    uint32_t *value1 = conf_getType(conf, "test", "key1", CONF_U32);
    uint32_t *value2 = conf_getType(conf, "test", "key2", CONF_U32);
    ceda_string_t **value3 = conf_getType(conf, "test", "key3", CONF_STR);

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
        {"test", "key2", CONF_STR, &values.value2},
        {NULL, NULL, CONF_NONE, NULL},
    };
    cr_assert_geq(ini_parse("test/conf/overwrite.ini", conf_handler, conf), 0);

    cr_assert_eq(values.value0, false);
    cr_assert_eq(values.value1, 2712);
    ceda_string_t *expected_string = ceda_string_new(0);
    ceda_string_cpy(expected_string, "hello new world");
    cr_assert(ceda_string_eq(values.value2, expected_string));
    ceda_string_delete(expected_string);
}

#endif
