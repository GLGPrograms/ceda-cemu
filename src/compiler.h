#ifndef CEDA_COMPILER_H
#define CEDA_COMPILER_H

/** Issue a compilation error if the \a condition is false */
#define STATIC_ASSERT(condition)                                               \
    extern char STATIC_ASSERTION_FAILED__[(condition) ? 1 : -1]

/**
 * Issue a compilation error if \a __cond is false (this can be used inside an
 * expression).
 */
#define STATIC_ASSERT_EXPR(__cond)                                             \
    (sizeof(struct { int STATIC_ASSERTION_FAILED__ : !!(__cond); }) * 0)

#ifndef countof
/**
 * Count the number of elements in the static array \a a.
 */
#if defined(__GNUC__) && !defined(__cplusplus)
/*
 * Perform a compile time type checking: countof() can only
 * work with static arrays, so throw a compile time error if a
 * pointer is passed as argument.
 *
 * NOTE: the construct __builtin_types_compatible_p() is only
 * available for C.
 */
#define countof(a)                                                             \
    (sizeof(a) / sizeof(*(a)) +                                                \
     STATIC_ASSERT_EXPR(                                                       \
         !__builtin_types_compatible_p(typeof(a), typeof(&a[0]))))
#else
#define countof(a) (sizeof(a) / sizeof(*(a)))
#endif
#endif

#endif // CEDA_COMPILER_H
