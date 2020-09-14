#ifndef INC_UART_H
#define INC_UART_H

void uart_init(void);
void uart_putchar(int);
char uart_getchar();
void uart_puts(char *);

#endif  /* !INC_UART_H */
