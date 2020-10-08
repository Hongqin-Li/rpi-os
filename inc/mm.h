#ifndef INC_MM_H
#define INC_MM_H

#include <stddef.h>

void mm_init();
void *kalloc();
void kfree(void *v);

#endif
