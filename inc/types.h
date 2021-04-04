#ifndef INC_TYPES_H
#define INC_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t ssize_t;

/* Efficient min and max operations */
#define MIN(_a, _b)                 \
({                                  \
    typeof(_a) __a = (_a);          \
    typeof(_b) __b = (_b);          \
    __a <= __b ? __a : __b;         \
})
#define MAX(_a, _b)                 \
({                                  \
    typeof(_a) __a = (_a);          \
    typeof(_b) __b = (_b);          \
    __a >= __b ? __a : __b;         \
})


/* Round down and up to the nearest multiple of n */
#define ROUNDDOWN(a, n)                 \
({                                      \
    uint64_t __a = (uint64_t) (a);      \
    (typeof(a)) (__a - __a % (n));      \
})
#define ROUNDUP(a, n)                                           \
({                                                              \
    uint64_t __n = (uint64_t) (n);                              \
    (typeof(a)) (ROUNDDOWN((uint64_t) (a) + __n - 1, __n));     \
})

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define IS_POWER_OF_2(n)    (!((n) & ((n)-1)))

#define container_of(ptr, type, member)                 \
({                                                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);  \
    (type *)((char *)__mptr - offsetof(type,member));   \
})

#endif
