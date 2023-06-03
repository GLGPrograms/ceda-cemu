#ifndef CEDA_MACRO_H
#define CEDA_MACRO_H

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b)          (((a) < (b)) ? (a) : (b))
#define MAX(a, b)          (((a) > (b)) ? (a) : (b))
#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#endif // CEDA_MACRO_H
