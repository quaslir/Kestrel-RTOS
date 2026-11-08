#ifndef KESTREL_QUEUE_H
#define KESTREL_QUEUE_H
#include <stdint.h>
#include "kestrel/wait_list_types.h"
#include "kestrel/task.h"
typedef struct {
    uint8_t *storage;
    uint32_t item_size, capacity;
    uint32_t head, tail, count;
    kestrel_wait_list_t receivers, senders;
} kestrel_queue_t;

kestrel_status_t kestrel_queue_init(kestrel_queue_t *q, void *storage, uint32_t item_size,
                                    uint32_t capacity);
kestrel_status_t kestrel_queue_send(kestrel_queue_t *q, const void *item, uint32_t timeout);
kestrel_status_t kestrel_queue_receive(kestrel_queue_t *q, void *buffer, uint32_t timeout);

#endif
