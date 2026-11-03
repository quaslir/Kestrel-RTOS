#include "kestrel/task.h"
#include "port.h"
#include <stdbool.h>
#include "kestrel/critical.h"
static uint8_t task_count = 0;
__attribute__((aligned(8))) static uint32_t stacks[MAX_TASKS][STACK_SIZE];
static kestrel_tcb_t tcbs[MAX_TASKS];
volatile kestrel_tcb_t *kestrel_current = NULLPTR;
__attribute__((noreturn)) static void trap(void)
{
    while (1) {
        __asm volatile("wfi");
    }
}
static void write_null(uint32_t **preg, uint8_t times)
{
    uint32_t *reg = *preg;
    for (uint8_t i = 0; i < times; i++) {
        *--reg = NULLPTR;
    }

    *preg = reg;
}

kestrel_task_result_t kestrel_new_task(kestrel_task_fn_t task, void *data)
{
    kestrel_task_result_t result;
    result.id = KESTREL_TASK_INVALID;
    if (!task) {
        result.status = KESTREL_ERR_NULL_FN;
        return result;
    }

    if (task_count == MAX_TASKS) {
        result.status = KESTREL_ERR_NO_SLOT;
        return result;
    }

    uint32_t *sp = &stacks[task_count][STACK_SIZE];
    *--sp = INITIAL_XPSR;
    *--sp = (uint32_t)task & CLEAR_BIT_ZERO;
    *--sp = (uint32_t)&trap;

    write_null(&sp, 4);

    *--sp = (uint32_t)data;
    write_null(&sp, 8);
    result.status = KESTREL_OK;
    result.id = task_count;
    tcbs[task_count++].sp = sp;

    return result;
}

void kestrel_start(void)
{
    if (task_count == 0) {
        trap();
    }
    kestrel_current = &tcbs[0];
    port_init();
    __asm volatile("svc 0"); // enter the interrupt handler mode

    trap();
}

void kestrel_schedule(void)
{
    if (task_count == 0) {
        return;
    }
    uint8_t current_idx = kestrel_current - tcbs;
    uint8_t next_idx = (current_idx + 1) % task_count;
    kestrel_current = &tcbs[next_idx];
}

void kestrel_yield(void)
{
    port_yield();
}

uint32_t kestrel_enter_critical(void)
{
    return port_enter_critical();
}
void kestrel_exit_critical(uint32_t prev)
{
    port_exit_critical(prev);
}
