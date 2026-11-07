#ifndef KESTREL_MUTEX_H
#define KESTREL_MUTEX_H
#include "wait_list_types.h"
#include "task.h"
typedef struct {
    struct kestrel_tcb *owner;
    kestrel_wait_list_t waiters;
} kestrel_mutex_t;

kestrel_status_t kestrel_mutex_init(kestrel_mutex_t *mutex);
kestrel_status_t kestrel_mutex_lock(kestrel_mutex_t *mutex, uint32_t timeout);
kestrel_status_t kestrel_mutex_unlock(kestrel_mutex_t *mutex);

#endif
