#include "tokenizer.h"

#include "macro.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#define LINE_BUFFER_SIZE 256

/**
 * @brief Extract the first word from a null-terminated C string.
 *
 * @param word Pointer to destination null-terminated string.
 * @param src Pointer to input string to inspect.
 * @param size Size of destination word buffer.
 *
 * @return const char* Pointer to first char after the word in the input string.
 * NULL if there are no more words.
 */
const char *tokenizer_next_word(char *word, const char *src, size_t size) {
    bool started = false;

    assert(src);
    if (*src == '\0') {
        return NULL;
    }

    size_t idx = 0; // index of current char under examination
    size_t len = 0; // toekn/word length
    for (; len < size - 1; ++idx) {
        if (src[idx] == ' ') {
            if (started) {
                break;
            }
            continue;
        }
        started = true;
        word[len++] = src[idx];

        if (src[idx] == '\0') {
            break;
        }
    }

    word[len++] = '\0';
    return src + idx;
}

/**
 * @brief Extract an unsigned int expressed in hex format from a C string.
 *
 * @param dst Pointer to unsigned int to write into.
 * @param src Pointer to input string to inspect.

 * @return const char* Pointer to first char after the unsigned int in the input
 * string. NULL if there has been an error during integer parsing.
 */
const char *tokenizer_next_hex(unsigned int *dst, const char *src) {
    if (src == NULL)
        return NULL;

    char *endptr = NULL;
    char word[LINE_BUFFER_SIZE] = {0};
    src = tokenizer_next_word(word, src, LINE_BUFFER_SIZE);
    if (src == NULL) {
        return NULL;
    }
    // TODO(giomba)
    // sooner or later, better if the handle hex8, hex16, hex32 separately
    *dst = (unsigned int)strtol(word, &endptr, 0x10);
    if (errno == EINVAL || errno == ERANGE || endptr == word) {
        return NULL;
    }

    return src;
}

/**
 * @brief Extract an unsigned int express in dec format from a C string.
 *
 * @param dst Pointer to unsigned int to write into.
 * @param src Pointer to input string to inspect.

 * @return const char* Pointer to first char after the unsigned int in the input
 * string. NULL if there has been an error during integer parsing.
 */
const char *tokenizer_next_int(unsigned int *dst, const char *src) {
    assert(src);
    static const int base = 10;

    char *endptr = NULL;
    char word[LINE_BUFFER_SIZE] = {0};
    src = tokenizer_next_word(word, src, LINE_BUFFER_SIZE);
    if (src == NULL) {
        return NULL;
    }

    *dst = (unsigned int)strtol(word, &endptr, base);
    if (errno == EINVAL || errno == ERANGE || endptr == word) {
        return NULL;
    }

    return src;
}

#if defined(CEDA_TEST)

#include <criterion/criterion.h>

Test(tokenizer, next_word) {
    const char *prompt = "   The quick  brown   fox";
    const char *words[] = {"The", "quick", "brown", "fox"};

    // check tokenize capability
    char word[LINE_BUFFER_SIZE];
    for (size_t i = 0; i < ARRAY_SIZE(words); ++i) {
        prompt = tokenizer_next_word(word, prompt, LINE_BUFFER_SIZE);
        cr_assert_str_eq(word, words[i]);
    }

    // no more words
    prompt = tokenizer_next_word(word, prompt, LINE_BUFFER_SIZE);
    cr_assert_eq(prompt, NULL);

    // check length constraints
    const size_t constraint = 6;
    tokenizer_next_word(word, "supercalifragilisticexpialidocious", constraint);
    cr_assert_str_eq(word, "super");
}

Test(tokenizer, next_hex) {
    const char *prompt = " 12 ab xx 77 ";
    const unsigned int values[] = {0x12, 0xab};

    unsigned int value = 0;
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
        prompt = tokenizer_next_hex(&value, prompt);
        cr_assert_eq(value, values[i]);
    }

    prompt = tokenizer_next_hex(&value, prompt);
    cr_assert_eq(prompt, NULL);
}

Test(tokenizer, next_int) {
    const char *prompt = "12 432 7a a7";
    const unsigned int values[] = {12, 432, 7};

    unsigned int value;
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
        prompt = tokenizer_next_int(&value, prompt);
        cr_assert_eq(value, values[i]);
    }

    prompt = tokenizer_next_int(&value, prompt);
    cr_assert_eq(prompt, NULL);
}

#endif