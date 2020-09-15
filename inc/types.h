#ifndef INC_TYPES_H
#define INC_TYPES_H

#include <stdint.h>

// Efficient min and max operations
#define MIN(_a, _b)               \
({                                \
  typeof(_a) __a = (_a);          \
  typeof(_b) __b = (_b);          \
  __a <= __b ? __a : __b;         \
})
#define MAX(_a, _b)               \
({                                \
  typeof(_a) __a = (_a);          \
  typeof(_b) __b = (_b);          \
  __a >= __b ? __a : __b;         \
})


// Rounding operations (efficient when n is a power of 2)
// Round down to the nearest multiple of n
#define ROUNDDOWN(a, n)                 \
({                                      \
  uint64_t __a = (uint64_t) (a);        \
  (typeof(a)) (__a - __a % (n));        \
})
// Round up to the nearest multiple of n
#define ROUNDUP(a, n)                                       \
({                                                          \
  uint64_t __n = (uint64_t) (n);                            \
  (typeof(a)) (ROUNDDOWN((uint64_t) (a) + __n - 1, __n));   \
})

#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))

#define OFFSET_OF(type, member) ((size_t) &((type *)0)->member)
#define CONTAINER_OF(ptr, type, member)                     \
({                                                          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - OFFSET_OF(type,member) );    \
})

#endif  /* !INC_TYPES_H */
