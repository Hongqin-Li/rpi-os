#include <stdarg.h>
#include <stdint.h>

#include "uart.h"

int panicked;

static void
printint(int xx, int base, int sign)
{
    static char digits[] = "0123456789abcdef";
    char buf[16];
    int i;
    uint32_t x;

    if(sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do{
        buf[i++] = digits[x % base];
    }while((x /= base) != 0);

    if(sign)
        buf[i++] = '-';

    while(--i >= 0)
        uart_putchar(buf[i]);
}

void
vprintfmt(void (*putch)(int), char *fmt, va_list ap)
{
    int i, c;
    char *s;
    for(i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if(c != '%') {
            putch(c);
            continue;
        }
        if (!(c = fmt[++i] & 0xff))
            break;

        switch(c) {
        case 'u':
            printint(va_arg(ap, int), 10, 0);
            break;
        case 'd':
            printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
        case 'p':
            printint(va_arg(ap, int), 16, 0);
            break;
        case 'c':
            putch(va_arg(ap, int));
            break;
        case 's':
            if((s = (char*)va_arg(ap, char *)) == 0)
                s = "(null)";
            for(; *s; s++)
                putch(*s);
            break;
        case '%':
            putch('%');
            break;
        default:
            // Print unknown % sequence to draw attention.
            putch('%');
            putch(c);
            break;
        }
    }
}

// Print to the console.
void
cprintf(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);
}

void
panic(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintfmt(uart_putchar, fmt, ap);
    va_end(ap);

    cprintf("%s:%d: kernel panic.\n", __FILE__, __LINE__);
    panicked = 1;
    while(1)
        ;
}
