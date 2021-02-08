#include "arm.h"
#include "spinlock.h"
#include "console.h"

#ifdef KERNLOCK
struct spinlock kernlock = {0};
#endif

void
initlock(struct spinlock *lk)
{
    lk->locked = 0;
}

void 
acquire(struct spinlock *lk)
{
#ifndef KERNLOCK
    disb();
    while (lk->locked || __atomic_test_and_set(&lk->locked, __ATOMIC_ACQUIRE))
        ;
    disb();
#endif
}

void 
release(struct spinlock *lk)
{
#ifndef KERNLOCK
    if (!lk->locked)
        panic("release: not locked\n");
    disb();
    __atomic_clear(&lk->locked, __ATOMIC_RELEASE);
    disb();
#endif
}

void
acquire_kern()
{
#ifdef KERNLOCK
    while (kernlock.locked || __atomic_test_and_set(&kernlock.locked, __ATOMIC_ACQUIRE))
        ;
#endif
}

void
release_kern()
{
#ifdef KERNLOCK
    __atomic_clear(&kernlock.locked, __ATOMIC_RELEASE);
#endif
}

