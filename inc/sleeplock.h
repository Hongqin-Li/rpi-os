#ifndef INC_SLEEPLOCK_H
#define INC_SLEEPLOCK_H

#include "spinlock.h"

/* Long-term locks for processes */
struct sleeplock {
    int locked;         /* Is the lock held? */
    struct spinlock lk; /* Spinlock protecting this sleep lock */
};

#endif
