#ifndef KESTREL_WAIT_TYPES_H
#define KESTREL_WAIT_TYPES_H

struct kestrel_tcb;

typedef struct kestrel_wait_list {
    struct kestrel_tcb *head;
} kestrel_wait_list_t;

#endif
