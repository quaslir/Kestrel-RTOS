#ifndef KESTREL_SEMAPHORE_H
#define KESTREL_SEMAPHORE_H
#include <stdint.h>
#include "wait_list_types.h"
#include "task.h"
typedef struct kestrel_sem_t {
    uint32_t max, count;
    kestrel_wait_list_t waiters;
} kestrel_sem_t;

kestrel_status_t kestrel_sem_init(kestrel_sem_t *sem, uint32_t initial, uint32_t max);
kestrel_status_t kestrel_sem_take(kestrel_sem_t *sem, uint32_t timeout);
kestrel_status_t kestrel_sem_give(kestrel_sem_t *sem);
#endif
