#ifndef KESTREL_READY_H
#define KESTREL_READY_H
#include "kestrel_internal.h"
#include <stdint.h>
#define KESTREL_MAX_PRIORITY 31
void ready_init(void);
kestrel_tcb_t *ready_pick(void);
uint32_t ready_get_mask(void);
void ready_insert(kestrel_tcb_t *tcb);
void ready_remove(kestrel_tcb_t *tcb);
bool ready_need_switch(uint8_t current_priority, bool current_is_idle);
#endif
