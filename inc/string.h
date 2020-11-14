#ifndef INC_STRING_H
#define INC_STRING_H

#include <stdint.h>
#include <stddef.h>

static inline void *
memset(void *str, int c, size_t n)
{
    char *l = (char *)str, *r = l + n;
    for (; l != r; l ++)
        *l = c & 0xff;
    return str;
}

static inline void *
memmove(void *dst, const void *src, size_t n)
{
    const char *s = src;
    char *d = dst;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0) *--d = *--s;
    } else {
        while (n-- > 0) *d++ = *s++;
    }

    return dst;
}

static inline int
memcmp(const void *v1, const void *v2, size_t n)
{
    for (const uint8_t *s1 = v1, *s2 = v2; n-- > 0; s1++, s2++)
        if  (*s1 != *s2)
            return *s1 - *s2;
    return 0;
}

#endif
