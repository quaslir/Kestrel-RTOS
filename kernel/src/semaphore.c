#include "kestrel/semaphore.h"
#include "kestrel/critical.h"
#include "kestrel_internal.h"
#include "wait_list.h"
#include "port.h"
kestrel_status_t kestrel_sem_init(kestrel_sem_t *sem, uint32_t initial, uint32_t max)
{
    if (!sem || max == 0 || initial > max) {
        return KESTREL_ERR_INVALID_ARG;
    }

    sem->count = initial;
    sem->max = max;
    wait_list_init(&sem->waiters);
    return KESTREL_OK;
}
kestrel_status_t kestrel_sem_take(kestrel_sem_t *sem, uint32_t timeout)
{
    if (!sem) {
        return KESTREL_ERR_INVALID_ARG;
    }

    kestrel_status_t result = KESTREL_OK;
    bool blocked = false;
    KESTREL_CRITICAL {
        if (sem->count > 0) {
            sem->count--;
        }

        else if (timeout == 0) {
            result = KESTREL_ERR_TIMEOUT;
        }

        else {
            if (port_in_isr()) {
                result = KESTREL_ERR_IN_ISR;
            } else {
                kestrel_block_on(&sem->waiters, timeout);
                blocked = true;
            }
        }
    }

    if (blocked) {
        result = kestrel_current->wait_result;
    }

    return result;
}
kestrel_status_t kestrel_sem_give(kestrel_sem_t *sem)
{
    if (!sem) {
        return KESTREL_ERR_INVALID_ARG;
    }

    kestrel_status_t result = KESTREL_OK;
    KESTREL_CRITICAL {
        kestrel_tcb_t *tcb = kestrel_wake_one(&sem->waiters);
        if (!tcb && sem->count < sem->max) {
            sem->count++;
        } else if (!tcb) {
            result = KESTREL_ERR_OVERFLOW;
        }
    }

    return result;
}
