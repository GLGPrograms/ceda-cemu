#ifndef CEDA_MACRO_H
#define CEDA_MACRO_H

#include <assert.h>
#include <stdlib.h>

// TODO(giomba): maybe print a stack trace?
#define CEDA_STRONG_ASSERT_NEQ(a, b)                                           \
    ({                                                                         \
        static_assert(__builtin_types_compatible_p(typeof(a), typeof(b)),      \
                      "type mismatch");                                        \
        typeof(a) first = a;                                                   \
        typeof(b) second = b;                                                  \
        if (!(first != second)) {                                              \
            abort();                                                           \
        }                                                                      \
    })

#define CEDA_STRONG_ASSERT_VALID_PTR(p)                                        \
    ({                                                                         \
        if ((p) == NULL) {                                                     \
            abort();                                                           \
        }                                                                      \
    })

#define CEDA_STRONG_ASSERT_TRUE(b)                                             \
    ({                                                                         \
        if (!((b) == true)) {                                                  \
            abort();                                                           \
        }                                                                      \
    })

#define CEDA_STRONG_ASSERT_LT(a, b)                                            \
    ({                                                                         \
        static_assert(__builtin_types_compatible_p(typeof(a), typeof(b)),      \
                      "type mismatch");                                        \
        typeof(a) first = a;                                                   \
        typeof(b) second = b;                                                  \
        if (!(first < second)) {                                               \
            abort();                                                           \
        }                                                                      \
    })

// TODO(giomba): make this more type safe
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MIN(a, b)                                                              \
    ({                                                                         \
        static_assert(__builtin_types_compatible_p(typeof(a), typeof(b)),      \
                      "type mismatch");                                        \
        const typeof(a) first = a;                                             \
        const typeof(b) second = b;                                            \
        typeof(a) min;                                                         \
        if (first < second) {                                                  \
            min = first;                                                       \
        } else {                                                               \
            min = second;                                                      \
        }                                                                      \
        min;                                                                   \
    })

#define MAX(a, b)                                                              \
    ({                                                                         \
        static_assert(__builtin_types_compatible_p(typeof(a), typeof(b)),      \
                      "type mismatch");                                        \
        const typeof(a) first = a;                                             \
        const typeof(b) second = b;                                            \
        typeof(a) max;                                                         \
        if (first > second)                                                    \
            max = first;                                                       \
        else                                                                   \
            max = second;                                                      \
        max;                                                                   \
    })

#define CLAMP(x, min, max) (MIN(MAX(min, x), max))

#endif // CEDA_MACRO_H
