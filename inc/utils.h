#ifndef INC_UTILS_H
#define INC_UTILS_H

#include <stdint.h>

static inline void
delay(int32_t count)
{
	asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
		 : "=r"(count): [count]"0"(count) : "cc");
}

static inline void
put32(uint64_t p, uint32_t x)
{
  *(volatile uint32_t *)p = x;
}

static inline uint32_t
get32(uint64_t p)
{
  return *(volatile uint32_t *)p;
}
 
#endif  /* !INC_UTILS_H */
