#ifndef CEDA_STRING_H
#define CEDA_STRING_H

#include "type.h"

#include <stdbool.h>

typedef struct ceda_string_s ceda_string_t;

ceda_string_t *ceda_string_new(ceda_size_t capacity);

void ceda_string_printf(ceda_string_t *str, const char *fmt, ...);

void ceda_string_cat(ceda_string_t *str, const char *data);

const char *ceda_string_data(const ceda_string_t *str);

ceda_size_t ceda_string_len(const ceda_string_t *str);

void ceda_string_cpy(ceda_string_t *str, const char *other);

bool ceda_string_eq(const ceda_string_t *left, const ceda_string_t *right);

void ceda_string_delete(ceda_string_t *str);

#endif
