#ifndef KESTREL_INTERNAL_H
#define KESTREL_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>

struct kestrel_wait_list;

typedef enum { KESTREL_TASK_READY, KESTREL_TASK_SLEEPING, KESTREL_TASK_DEAD } kestrel_task_state_t;
typedef struct kestrel_tcb {
    uint32_t *sp;
    kestrel_task_state_t state;
    uint32_t wake_tick;
    uint8_t priority; // higher the number - higher priority
    struct kestrel_tcb *next;
    struct kestrel_tcb *prev;
    struct kestrel_wait_list *waiting_on;
} kestrel_tcb_t;

extern volatile bool kestrel_started;
extern kestrel_tcb_t *volatile kestrel_current;
bool kestrel_tick(void);
void kestrel_schedule(void);
void kestrel_wake_sleeping(uint32_t now);

#endif
