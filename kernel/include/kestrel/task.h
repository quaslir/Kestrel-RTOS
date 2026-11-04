#ifndef KESTREL_TASK_H
#define KESTREL_TASK_H
#include <stdint.h>
#include <stdbool.h>
#define MAX_TASKS            8
#define STACK_SIZE           256
#define KESTREL_TASK_INVALID 0xFFU
#define NULLPTR              0x0
#define CLEAR_BIT_ZERO       (~(1U << 0))
#define INITIAL_XPSR         (1UL << 24)
#define KESTREL_MAX_PRIORITY 31
typedef void (*kestrel_task_fn_t)(void *arg);

typedef enum {
    KESTREL_OK = 0,
    KESTREL_ERR_NO_SLOT = -1,
    KESTREL_ERR_NULL_FN = -2,
    KESTREL_ERR_INVALID_PRIORITY = -3,
} kestrel_status_t;

typedef enum { KESTREL_TASK_READY, KESTREL_TASK_SLEEPING } kestrel_task_state_t;
typedef struct kestrel_tcb {
    uint32_t *sp;
    kestrel_task_state_t state;
    uint32_t wake_tick;
    uint8_t priority; // higher the number - higher priority
    struct kestrel_tcb *next;
    struct kestrel_tcb *prev;
} kestrel_tcb_t;

typedef struct {
    kestrel_status_t status;
    uint8_t id;
} kestrel_task_result_t;

kestrel_task_result_t kestrel_new_task(kestrel_task_fn_t task, void *data, uint8_t priority);
__attribute__((noreturn)) void kestrel_start(void);
void kestrel_yield(void);
void kestrel_delay(uint32_t ticks);
void kestrel_delay_until(uint32_t *last_wake, uint32_t period);
void kestrel_wake_sleeping(uint32_t now);
#endif
