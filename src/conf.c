#include "conf.h"

#include "ceda_string.h"
#include "macro.h"
#include "tokenizer.h"
#include "type.h"

#include <ini.h>

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static conf_tuple_t conf_tuples[] = {
    {"mod", "cge_installed", CONF_BOOL, &conf.cge_installed},
    {"mod", "charmon_installed", CONF_BOOL, &conf.charmon_installed},
    {"path", "bios_rom", CONF_STR, (void *)&conf.bios_rom_path},
    {"path", "char_rom", CONF_STR, (void *)&conf.char_rom_path},
    {"path", "cge_rom", CONF_STR, (void *)&conf.cge_rom_path},
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
                *((bool *)(tuple->value)) = (bool)n;
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

    // load ini from user home
    const char *home = getenv("HOME");
    if (home) {
        (void)snprintf(path, CONF_PATH_SIZE, "%s/%s", home, CONF_PATH_HOME);
        if (ini_parse(path, conf_handler, conf_tuples) >= 0)
            loaded_path = path;
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
    ceda_string_t **string =
        (ceda_string_t **)conf_getType(conf_tuples, section, key, CONF_STR);

    if (string == NULL || *string == NULL)
        return NULL;

    return ceda_string_data(*string);
}

ini_handler conf_testGetHandler(void) {
    return conf_handler;
}

conf_getType_t conf_testGetGetType(void) {
    return conf_getType;
}
