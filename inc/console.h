#ifndef INC_CONSOLE_H
#define INC_CONSOLE_H

#include <stdarg.h>
#include "spinlock.h"

extern struct spinlock dbglock;

void console_init();
void cprintf1(const char *fmt, ...);
void cprintf(const char *fmt, ...);
void panic(const char *fmt, ...);

#define assert(x)                                                   \
({                                                                  \
    if (!(x)) {                                                     \
        panic("%s:%d: assertion failed.\n", __FILE__, __LINE__);    \
    }                                                               \
})

#define asserts(x, ...)                                             \
({                                                                  \
    if (!(x)) {                                                     \
        cprintf("%s:%d: assertion failed.\n", __FILE__, __LINE__);  \
        panic(__VA_ARGS__);                                         \
    }                                                               \
})

#define LOG1(level, ...)                        \
({                                              \
    acquire(&dbglock);                          \
    cprintf1("[%d]%s: ", cpuid(), __func__);    \
    cprintf1(__VA_ARGS__);                      \
    cprintf1("\n");                             \
    release(&dbglock);                          \
})

#ifdef LOG_ERROR
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_WARN)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_INFO)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)
#define trace(...)

#elif defined(LOG_DEBUG)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)

#elif defined(LOG_TRACE)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)  LOG1("TRACE", __VA_ARGS__);

#else
/* Default to none. */
#define error(...)
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#endif

#endif
