#include "kestrel/mutex.h"
#include "kestrel/critical.h"
#include "wait_list.h"
#include "kestrel_internal.h"
#include "port.h"
#include <stddef.h>

kestrel_status_t kestrel_mutex_init(kestrel_mutex_t *mutex)
{
    if (!mutex) {
        return KESTREL_ERR_INVALID_ARG;
    }

    mutex->owner = NULL;
    wait_list_init(&mutex->waiters);

    return KESTREL_OK;
}
kestrel_status_t kestrel_mutex_unlock(kestrel_mutex_t *mutex)
{
    if (!mutex) {
        return KESTREL_ERR_INVALID_ARG;
    }
    if (port_in_isr()) {
        return KESTREL_ERR_IN_ISR;
    }
    kestrel_status_t result = KESTREL_OK;
    KESTREL_CRITICAL {
        if (mutex->owner != kestrel_current) {
            result = KESTREL_ERR_NOT_OWNER;
        } else {
            kestrel_set_priority(kestrel_current, kestrel_current->base_priority);
            kestrel_tcb_t *next = kestrel_wake_one(&mutex->waiters);
            mutex->owner = next;
        }
    }

    return result;
}
kestrel_status_t kestrel_mutex_lock(kestrel_mutex_t *mutex, uint32_t timeout)
{
    if (!mutex) {
        return KESTREL_ERR_INVALID_ARG;
    }

    if (port_in_isr()) {
        return KESTREL_ERR_IN_ISR;
    }

    kestrel_status_t result = KESTREL_OK;

    bool blocked = false;
    KESTREL_CRITICAL {
        if (!mutex->owner) {
            mutex->owner = kestrel_current;
        } else if (mutex->owner == kestrel_current) {
            result = KESTREL_ERR_INVALID_ARG;
        } else if (timeout == KESTREL_NO_WAIT) {
            result = KESTREL_ERR_TIMEOUT;
        }

        else {
#if KESTREL_CFG_PRIORITY_INHERITANCE
            if (mutex->owner->priority < kestrel_current->priority) {
                kestrel_set_priority(mutex->owner, kestrel_current->priority);
            }
#endif
            kestrel_block_on(&mutex->waiters, timeout);
            blocked = true;
        }
    }

    if (blocked) {
        result = kestrel_current->wait_result;
    }
    return result;
}
