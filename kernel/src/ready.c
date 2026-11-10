#include "ready.h"
#include "kestrel_internal.h"
#include <stddef.h>
#include "port.h"
static uint32_t ready_mask = 0U;
static kestrel_tcb_t *ready_head[KESTREL_MAX_PRIORITY + 1];

void ready_init(void)
{
    ready_mask = 0U;
    for (uint8_t i = 0; i < KESTREL_MAX_PRIORITY + 1; i++) {
        ready_head[i] = NULL;
    }
}
kestrel_tcb_t *ready_pick(void)
{
    if (ready_mask == 0) {
        return NULL;
    }
    uint8_t prio = port_highest_priority(ready_mask);
    kestrel_tcb_t *next_task = ready_head[prio];
    ready_head[prio] = next_task->next;

    return next_task;
}
uint32_t ready_get_mask(void)
{
    return ready_mask;
}

void ready_insert(kestrel_tcb_t *tcb)
{
    kestrel_tcb_t **head = &ready_head[tcb->priority];
    if (!*head) {
        tcb->next = tcb;
        tcb->prev = tcb;
        *head = tcb;
    } else {
        kestrel_tcb_t *prev = (*head)->prev;
        prev->next = tcb;
        tcb->next = *head;
        tcb->prev = prev;
        (*head)->prev = tcb;
    }

    ready_mask |= (1U << tcb->priority);
}

void ready_remove(kestrel_tcb_t *tcb)
{
    kestrel_tcb_t *prev = tcb->prev;
    prev->next = tcb->next;
    tcb->next->prev = prev;
    kestrel_tcb_t **head = &ready_head[tcb->priority];
    if (tcb->next == tcb) {
        ready_mask &= ~(1U << tcb->priority);
        *head = NULL;
    } else if (tcb == *head) {
        *head = tcb->next;
    }
    tcb->next = NULL;
    tcb->prev = NULL;
}

bool ready_need_switch(uint8_t current_priority, bool current_is_idle)
{
    if (ready_mask == 0) {
        return false;
    }
    if (current_is_idle) {
        return true;
    }
    uint8_t top = port_highest_priority(ready_mask);
    if (top > current_priority) {
        return true;
    }
    if (top == current_priority && ready_head[top] != ready_head[top]->next) {
        return true;
    }

    return false;
}
