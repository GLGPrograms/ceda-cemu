#ifndef CEDA_CONF_H
#define CEDA_CONF_H

#include <stdbool.h>
#include <stdint.h>

#include <ini.h>

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

void conf_init(void);

uint32_t *conf_getU32(const char *section, const char *key);
bool *conf_getBool(const char *section, const char *key);
const char *conf_getString(const char *section, const char *key);

void conf_cleanup(void);

// Test specific interface
ini_handler conf_testGetHandler(void);
typedef void *(*conf_getType_t)(conf_tuple_t *tuples, const char *section,
                                const char *key, conf_type_t type);
conf_getType_t conf_testGetGetType(void);

#endif // CEDA_CONF_H
