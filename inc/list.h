#ifndef INC_LIST_H
#define INC_LIST_H

#include "types.h"

struct list_head {
    struct list_head *next, *prev; 
};

static inline void
list_init(struct list_head *head) 
{
    head->next = head->prev = head;
}

static inline int 
list_empty(struct list_head *head) 
{
    return head->next == head;
}
static inline struct list_head *
list_front(struct list_head *head)
{
    return head->next;
}
static inline struct list_head *
list_back(struct list_head *head) 
{
    return head->prev;
}

static inline void 
list_insert(struct list_head *cur, struct list_head *prev, struct list_head *next)
{
    next->prev = cur;
    cur->next = next;
    cur->prev = prev;
    prev->next = cur;
}

static inline void 
list_push_front(struct list_head *head, struct list_head *cur) 
{
    list_insert(cur, head, head->next);
}

static inline void 
list_push_back(struct list_head *head, struct list_head *cur)
{
    list_insert(cur, head->prev, head);
}

static inline void
list_del(struct list_head *prev, struct list_head *next) 
{
    next->prev = prev;
    prev->next = next;
}

static inline void
list_drop(struct list_head *item) 
{
    list_del(item->prev, item->next);
}

static inline void
list_pop_front(struct list_head *head)
{
    list_drop(list_front(head));
}

static inline void
list_pop_back(struct list_head *head)
{
    list_drop(list_back(head));
}

static inline struct list_head *
list_find(struct list_head *head, struct list_head *item)
{
    for (struct list_head *p = head->next; p != head; p = p->next) {
        if (p == item) 
            return item;
    }
    return 0;
}

#define LIST_FOREACH_ENTRY(pos, head, member)                           \
    for (pos = container_of(list_front(head), typeof(*pos), member);    \
        &pos->member != (head);                                         \
        pos = container_of(pos->member.next, typeof(*pos), member))

#define LIST_FOREACH_ENTRY_REVERSE(pos, head, member)                   \
    for (pos = container_of(list_back(head), typeof(*pos), member);     \
        &pos->member != (head);                                         \
        pos = container_of(pos->member.prev, typeof(*pos), member))

/* Iterate over a list safe against removal of list entry. */
#define LIST_FOREACH_ENTRY_SAFE(pos, n, head, member)                   \
    for(pos = container_of(list_front(head), typeof(*pos), member),     \
        n = container_of(pos->member.next, typeof(*n), member);         \
        &pos->member != (head);                                         \
        pos = n, n = container_of(n->member.next, typeof(*n), member))

#endif
