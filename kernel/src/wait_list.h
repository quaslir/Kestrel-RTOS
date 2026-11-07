#ifndef KESTREL_WAIT_H
#define KESTREL_WAIT_H
#include <stdbool.h>
#include "kestrel/wait_list_types.h"

void wait_list_init(kestrel_wait_list_t *list);
void wait_list_insert(kestrel_wait_list_t *list, struct kestrel_tcb *tcb);
void wait_list_remove(kestrel_wait_list_t *list, struct kestrel_tcb *tcb);
struct kestrel_tcb *wait_list_pop(kestrel_wait_list_t *list);
bool wait_list_is_empty(kestrel_wait_list_t *list);

#endif
