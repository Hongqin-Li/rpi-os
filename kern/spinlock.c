#include "spinlock.h"
#include "console.h"

void 
acquire(struct spinlock *lk)
{
    while(lk->locked || __sync_lock_test_and_set(&lk->locked, 1))
        ;
}

void 
release(struct spinlock *lk)
{
    if (!lk->locked)
        panic("release: not locked\n");
    __sync_lock_test_and_set(&lk->locked, 0);
}
