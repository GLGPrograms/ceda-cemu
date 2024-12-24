#ifndef CEDA_TOKENIZER_H
#define CEDA_TOKENIZER_H

#include <stddef.h>

const char *tokenizer_next_word(char *word, const char *src, size_t size);
const char *tokenizer_next_hex(unsigned int *dst, const char *src);
const char *tokenizer_next_int(unsigned int *dst, const char *src);

#endif // CEDA_TOKENIZER_H
