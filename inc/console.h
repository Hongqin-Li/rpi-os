#ifndef INC_CONSOLE_H
#define INC_CONSOLE_H

#include <stdarg.h>
#include <stdint.h>

int cprintf(const char *fmt, ...);
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);
int	snprintf(char *str, int size, const char *fmt, ...);
int	vsnprintf(char *str, int size, const char *fmt, va_list);

void panic(const char *fmt, ...);
#define assert(x)  { if (!(x)) { cprintf("%s:%d: assertion failed.\n", __FILE__, __LINE__); while(1); }  }

#endif  /* !INC_CONSOLE_H */
