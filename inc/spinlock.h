#ifndef INC_SPINLOCK_H
#define INC_SPINLOCK_H

struct spinlock {
    volatile int locked;
};
void initlock(struct spinlock *);
void acquire(struct spinlock *);
void release(struct spinlock *);

#ifdef KERNLOCK
void acquire_kern();
void release_kern();
#endif

#endif
