#include "wait_list.h"
#include <stddef.h>
void wait_list_init(kestrel_wait_list_t *list)
{
    list->head = NULL;
}
void wait_list_insert(kestrel_wait_list_t *list, kestrel_tcb_t *tcb)
{
    kestrel_tcb_t **pp = &list->head;
    kestrel_tcb_t *prev = NULL;

    while (*pp && (*pp)->priority >= tcb->priority) {
        prev = *pp;
        pp = &(*pp)->next;
    }

    tcb->next = *pp;
    tcb->prev = prev;
    if (*pp) {
        (*pp)->prev = tcb;
    }
    *pp = tcb;
    tcb->waiting_on = list;
}
void wait_list_remove(kestrel_wait_list_t *list, kestrel_tcb_t *tcb)
{
    if (wait_list_is_empty(list) || !tcb) {
        return;
    }
    kestrel_tcb_t **head = &list->head;
    if (tcb->prev) {
        tcb->prev->next = tcb->next;
    } else {
        *head = tcb->next;
    }

    if (tcb->next) {
        tcb->next->prev = tcb->prev;
    }

    tcb->next = NULL;
    tcb->prev = NULL;
    tcb->waiting_on = NULL;
}
kestrel_tcb_t *wait_list_pop(kestrel_wait_list_t *list)
{
    if (!list->head) {
        return NULL;
    }
    kestrel_tcb_t *task = list->head;
    wait_list_remove(list, list->head);
    return task;
}
bool wait_list_is_empty(kestrel_wait_list_t *list)
{
    return list->head == NULL;
}
