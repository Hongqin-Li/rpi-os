#ifndef INC_MM_H
#define INC_MM_H

#include <stddef.h>

void free_range(void *start, void *end);
void *kalloc(size_t sz);
void kfree(void *v);

#endif  /* !INC_MM_H */
