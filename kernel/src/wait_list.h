#ifndef KESTREL_WAIT_H
#define KESTREL_WAIT_H
#include <stdbool.h>

#include "kestrel_internal.h"

typedef struct kestrel_wait_list {
    kestrel_tcb_t *head;
} kestrel_wait_list_t;

void wait_list_init(kestrel_wait_list_t *list);
void wait_list_insert(kestrel_wait_list_t *list, kestrel_tcb_t *tcb);
void wait_list_remove(kestrel_wait_list_t *list, kestrel_tcb_t *tcb);
kestrel_tcb_t *wait_list_pop(kestrel_wait_list_t *list);
bool wait_list_is_empty(kestrel_wait_list_t *list);

#endif
