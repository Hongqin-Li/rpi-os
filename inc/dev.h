#ifndef INC_DEV_H
#define INC_DEV_H

#include "buf.h"

void dev_init();
void dev_intr();
void devrw(struct buf *);

#endif
