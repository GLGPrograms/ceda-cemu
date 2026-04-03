#include "../ceda_string.h"

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

Test(ceda_string, len) {
    ceda_string_t *str = ceda_string_new(0);
    cr_assert_eq(ceda_string_len(str), 0);
    ceda_string_delete(str);

    ceda_string_t *str2 = ceda_string_new(4);
    ceda_string_cpy(str2, "");
    cr_assert_eq(ceda_string_len(str2), 0);
    ceda_string_delete(str2);
}

Test(ceda_string, eq) {
    ceda_string_t *str0 = ceda_string_new(0);
    ceda_string_cpy(str0, "hello world");
    ceda_string_t *str1 = ceda_string_new(0);
    ceda_string_cpy(str1, "hello world");
    ceda_string_t *str2 = ceda_string_new(0);
    ceda_string_cpy(str2, "hello Earth");

    cr_assert(ceda_string_eq(str0, str1));
    cr_assert(!ceda_string_eq(str0, str2));

    ceda_string_delete(str0);
    ceda_string_delete(str1);
    ceda_string_delete(str2);
}
