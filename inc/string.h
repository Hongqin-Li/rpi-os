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
    const char *s = (const char*)src;
    char *d = (char*)dst;
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
    for (const uint8_t *s1 = (const uint8_t*)v1, *s2 = (const uint8_t*)v2; n-- > 0; s1++, s2++)
        if  (*s1 != *s2)
            return *s1 - *s2;
    return 0;
}

// #define uint uint32_t
// #define uchar uint8_t
// int
// strncmp(const char *p, const char *q, uint n)
// {
//   while(n > 0 && *p && *p == *q)
//     n--, p++, q++;
//   if(n == 0)
//     return 0;
//   return (uchar)*p - (uchar)*q;
// }

// char*
// strncpy(char *s, const char *t, int n)
// {
//   char *os;

//   os = s;
//   while(n-- > 0 && (*s++ = *t++) != 0)
//     ;
//   while(n-- > 0)
//     *s++ = 0;
//   return os;
// }

// // Like strncpy but guaranteed to NUL-terminate.
// char*
// safestrcpy(char *s, const char *t, int n)
// {
//   char *os;

//   os = s;
//   if(n <= 0)
//     return os;
//   while(--n > 0 && (*s++ = *t++) != 0)
//     ;
//   *s = 0;
//   return os;
// }

// int
// strlen(const char *s)
// {
//   int n;

//   for(n = 0; s[n]; n++)
//     ;
//   return n;
// }



#endif
