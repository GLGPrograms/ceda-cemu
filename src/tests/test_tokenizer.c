#include <criterion/criterion.h>

#include <stddef.h>

#include "../macro.h"
#include "../tokenizer.h"

#define LINE_BUFFER_SIZE 128

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
