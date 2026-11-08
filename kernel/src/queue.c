#include "kestrel/queue.h"
#include "kestrel_internal.h"
#include "kestrel/critical.h"
#include "wait_list.h"
#include <string.h>
#include "port.h"
kestrel_status_t kestrel_queue_init(kestrel_queue_t *q, void *storage, uint32_t item_size,
                                    uint32_t capacity)
{
    if (!q || !storage || item_size == 0 || capacity == 0) {
        return KESTREL_ERR_INVALID_ARG;
    }

    q->storage = (uint8_t *)storage;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->item_size = item_size;
    wait_list_init(&q->receivers);
    wait_list_init(&q->senders);
    return KESTREL_OK;
}
kestrel_status_t kestrel_queue_send(kestrel_queue_t *q, const void *item, uint32_t timeout)
{
    if (!q || !item) {
        return KESTREL_ERR_INVALID_ARG;
    }
    kestrel_status_t result = KESTREL_OK;
    bool blocked = false;
    KESTREL_CRITICAL
    {
        if (!wait_list_is_empty(&q->receivers)) {
            kestrel_tcb_t *tcb = kestrel_wake_one(&q->receivers);
            memcpy(tcb->wait_data, item, q->item_size);
        } else if (q->count < q->capacity) {
            memcpy(q->storage + (q->tail * q->item_size), item, q->item_size);

            q->tail = (q->tail + 1) % q->capacity;
            q->count++;
        } else if (timeout == KESTREL_NO_WAIT) {
            result = KESTREL_ERR_TIMEOUT;
        } else {
            if (port_in_isr()) {
                result = KESTREL_ERR_IN_ISR;
            } else {
                kestrel_current->wait_data = (void *)item;
                kestrel_block_on(&q->senders, timeout);
                blocked = true;
            }
        }
    }

    if (blocked) {
        result = kestrel_current->wait_result;
    }
    return result;
}
kestrel_status_t kestrel_queue_receive(kestrel_queue_t *q, void *buffer, uint32_t timeout)
{
    if (!q || !buffer) {
        return KESTREL_ERR_INVALID_ARG;
    }
    bool blocked = false;
    kestrel_status_t result = KESTREL_OK;
    KESTREL_CRITICAL
    {
        if (q->count > 0) {
            memcpy(buffer, q->storage + (q->head * q->item_size), q->item_size);
            q->head = (q->head + 1) % q->capacity;
            q->count--;
            if (!wait_list_is_empty(&q->senders)) {
                kestrel_tcb_t *tcb = kestrel_wake_one(&q->senders);
                memcpy(q->storage + (q->tail * q->item_size), tcb->wait_data, q->item_size);
                q->tail = (q->tail + 1) % q->capacity;
                q->count++;
            }
        } else if (timeout == KESTREL_NO_WAIT) {
            result = KESTREL_ERR_TIMEOUT;
        } else {
            if (port_in_isr()) {
                result = KESTREL_ERR_IN_ISR;
            } else {
                kestrel_current->wait_data = buffer;
                kestrel_block_on(&q->receivers, timeout);
                blocked = true;
            }
        }
    }
    if (blocked) {
        result = kestrel_current->wait_result;
    }

    return result;
}
