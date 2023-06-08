#include "ceda_string.h"

#include "macro.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CEDA_STRING_COUNT = 32,
    CEDA_STRING_DEFAULT_CAPACITY = 128,
    CEDA_STRING_PRINTF_BUFFER_SIZE = 1024,
};

typedef struct ceda_string_s {
    bool valid;           // does this struct contain meaningful data?
    ceda_size_t capacity; // maximum char capacity
    ceda_size_t used;     // used chars
    char *data;           // null-terminated C string
} ceda_string_t;

static ceda_string_t ceda_strings[CEDA_STRING_COUNT];

ceda_string_t *ceda_string_new(ceda_size_t capacity) {
    ceda_string_t *ret = NULL;
    if (capacity == 0) {
        capacity = CEDA_STRING_DEFAULT_CAPACITY;
    }

    for (ceda_size_t i = 0; i < (ceda_size_t)ARRAY_SIZE(ceda_strings); ++i) {
        ceda_string_t *str = &ceda_strings[i];
        if (str->valid) {
            continue;
        }

        str->data = malloc(capacity);
        if (str->data == NULL) {
            break;
        }
        str->data[0] = '\0';
        str->capacity = capacity;
        str->used = 1;
        str->valid = true;
        ret = str;
        break;
    }

    CEDA_STRONG_ASSERT_VALID_PTR(ret);

    return ret;
}

void ceda_string_cat(ceda_string_t *str, const char *data) {
    CEDA_STRONG_ASSERT_TRUE(str->valid);

    // better die than doing random stuff
    const size_t strlen_data = strlen(data);
    CEDA_STRONG_ASSERT_LT(strlen_data, (size_t)0x10000);

    const ceda_size_t needed = (ceda_size_t)strlen_data;
    const ceda_size_t remaining = str->capacity - str->used;
    if (needed > remaining) {
        // ceda_size_t is uint16_t under the hood,
        // so a string longer than 64K cannot be allocated
        CEDA_STRONG_ASSERT_LT(str->capacity, (ceda_size_t)0x8000);

        // double the amount of memory allocated for the ceda_string_t
        const uint16_t new_capacity = str->capacity * 2;
        char *new_data = malloc(new_capacity);
        CEDA_STRONG_ASSERT_VALID_PTR(new_data);

        // copy old data into new allocated memory
        char *old_data = str->data;
        for (uint16_t i = 0; i < str->used; ++i) {
            new_data[i] = old_data[i];
        }

        free(old_data);
        str->capacity = new_capacity;
        str->data = new_data;
    }

    // concatenate data, overwriting old null-terminator
    --str->used;
    for (ceda_size_t i = 0; i < strlen_data; ++i) {
        str->data[str->used++] = data[i];
    }
    str->data[str->used++] = '\0';
}

void ceda_string_printf(ceda_string_t *str, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    char buffer[CEDA_STRING_PRINTF_BUFFER_SIZE] = {0};

    // We need I/O, this has to be included at least once in the code base.
    // NOLINTNEXTLINE
    (void)vsnprintf(buffer, CEDA_STRING_PRINTF_BUFFER_SIZE, fmt, argp);

    ceda_string_cat(str, buffer);
    va_end(argp);
}

const char *ceda_string_data(const ceda_string_t *str) {
    CEDA_STRONG_ASSERT_TRUE(str->valid);

    return str->data;
}

void ceda_string_cpy(ceda_string_t *str, const char *other) {
    str->data[0] = '\0';
    str->used = 1;
    ceda_string_cat(str, other);
}

ceda_size_t ceda_string_len(const ceda_string_t *str) {
    return str->used;
}

void ceda_string_delete(ceda_string_t *str) {
    CEDA_STRONG_ASSERT_TRUE(str->valid);

    free(str->data);
    str->valid = false;
}

#ifdef CEDA_TEST

#include <criterion/criterion.h>

Test(ceda_string, new) {
    ceda_string_t *str = ceda_string_new(0);
    ceda_string_cat(str, "hello world");
    cr_assert_str_eq(ceda_string_data(str), "hello world");
    ceda_string_delete(str);
}

Test(ceda_string, concat) {
    ceda_string_t *str = ceda_string_new(0);
    ceda_string_cat(str, "hello ");
    ceda_string_cat(str, "world");
    cr_assert_str_eq(ceda_string_data(str), "hello world");
    ceda_string_delete(str);
}

Test(ceda_string, auto_alloc) {
    ceda_string_t *str = ceda_string_new(4);
    ceda_string_cat(str, "hello world ");
    ceda_string_cat(str, "everybody!");
    cr_assert_str_eq(ceda_string_data(str), "hello world everybody!");
    ceda_string_delete(str);
}

Test(ceda_string, printf) {
    const int magic = 0x55;
    ceda_string_t *str = ceda_string_new(0);
    ceda_string_printf(str, "%s %d ", "hello world", magic);
    ceda_string_printf(str, "%u %x %X", magic, magic, magic);
    cr_assert_str_eq(ceda_string_data(str), "hello world 85 85 55 55");
    ceda_string_delete(str);
}

Test(ceda_string, cpy) {
    ceda_string_t *str = ceda_string_new(0);
    ceda_string_cat(str, "hello");
    ceda_string_cpy(str, "world");
    cr_assert_str_eq(ceda_string_data(str), "world");
    ceda_string_delete(str);
}

#endif
