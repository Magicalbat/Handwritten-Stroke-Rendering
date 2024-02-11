#ifndef BASE_DEFS_H
#define BASE_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef i8  b8;
typedef i32 b32;

typedef float  f32;
typedef double f64;

static_assert(sizeof(f32) == 4, "f32 size");
static_assert(sizeof(f64) == 8, "f64 size");

#if defined(_WIN32)
#   define PLATFORM_WIN32
#elif defined(__linux__)
#   define PLATFORM_LINUX
#endif

#define UNUSED(x) (void)(x)

#define CONCAT_NX(a, b) a##b
#define CONCAT(a, b) CONCAT_NX(a, b)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ABS(n) ((n) < 0 ? -(n) : (n))
#define SIGN(n) ((n) < 0 ? -1 : 1)
#define CLAMP(x, a, b) (MIN((b), MAX((x), (a))))

#define SLL_PUSH_FRONT(f, l, n) ((f) == 0 ? \
    ((f) = (l) = (n)) :                     \
    ((n)->next = (f), (f) = (n)))           \

#define SLL_PUSH_BACK(f, l, n) ((f) == 0 ? \
    ((f) = (l) = (n)) :                    \
    ((l)->next = (n), (l) = (n)),          \
    ((n)->next = 0))                       \

#define SLL_POP_FRONT(f, l) ((f) == (l) ? \
    ((f) = (l) = 0) :                     \
    ((f) = (f)->next))                    \

#define DLL_PUSH_BACK(f, l, n) ((f) == 0 ? \
    ((f) = (l) = (n), (n)->next = (n)->prev = 0) :  \
    ((n)->prev = (l), (l)->next = (n), (l) = (n), (n)->next = 0))

#define DLL_PUSH_FRONT(f, l, n) DLL_PUSH_BACK(l, f, n)

#define DLL_REMOVE(f, l, n) ( \
    (f) == (n) ? \
        ((f) == (l) ? \
            ((f) = (l) = (0)) : \
            ((f) = (f)->next, (f)->prev = 0)) : \
        (l) == (n) ? \
            ((l) = (l)->prev, (l)->next = 0) : \
            ((n)->next->prev = (n)->prev, \
            (n)->prev->next = (n)->next))

#endif // BASE_DEFS_H
