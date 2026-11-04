#include "kestrel/task.h"
#include "port.h"
#include <stdbool.h>
#include "kestrel/critical.h"
#include "kestrel/time.h"
#define IDLE_STACK_SIZE 64

static uint8_t task_count = 0;
__attribute__((aligned(8))) static uint32_t stacks[MAX_TASKS][STACK_SIZE];
static kestrel_tcb_t tcbs[MAX_TASKS];
volatile kestrel_tcb_t *kestrel_current = NULLPTR;
__attribute__((aligned(8))) static uint32_t idle_stack[IDLE_STACK_SIZE];
static kestrel_tcb_t idle_tcb;
__attribute__((noreturn)) static void trap(void *arg)
{
    while (1) {
        __asm volatile("wfi");
    }
}

__attribute__((noreturn)) static void idle_task(void *arg)
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

static uint32_t *build_frame(uint32_t *stack_end, kestrel_task_fn_t fn, void *arg)
{
    uint32_t *sp = stack_end;
    *--sp = INITIAL_XPSR;
    *--sp = (uint32_t)fn & CLEAR_BIT_ZERO;
    *--sp = (uint32_t)&trap;

    write_null(&sp, 4);

    *--sp = (uint32_t)arg;
    write_null(&sp, 8);
    return sp;
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

    uint32_t *sp = build_frame(&stacks[task_count][STACK_SIZE], task, data);

    result.status = KESTREL_OK;
    result.id = task_count;
    tcbs[task_count].sp = sp;
    tcbs[task_count++].state = KESTREL_TASK_READY;
    return result;
}

void kestrel_start(void)
{
    if (task_count == 0) {
        trap(NULLPTR);
    }
    kestrel_current = &tcbs[0];
    port_init();
    idle_tcb.sp =
        build_frame(idle_stack + (sizeof(idle_stack) / sizeof(uint32_t)), &idle_task, NULLPTR);

    idle_tcb.state = KESTREL_TASK_READY;
    __asm volatile("svc 0"); // enter the interrupt handler mode

    trap(NULLPTR);
}

void kestrel_schedule(void)
{
    if (task_count == 0) {
        return;
    }
    uint8_t current_idx = 0;
    if (kestrel_current != &idle_tcb) {
        current_idx = (kestrel_current - tcbs + 1) % task_count;
    }
    uint8_t i = 0;

    while (i < task_count) {
        if (tcbs[current_idx].state == KESTREL_TASK_READY) {
            break;
        }
        current_idx = (current_idx + 1) % task_count;
        i++;
    }

    if (i == task_count) {
        kestrel_current = &idle_tcb;
        return;
    }
    kestrel_current = &tcbs[current_idx];
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

void kestrel_delay(uint32_t ticks)
{
    if (ticks == 0 || !kestrel_current) {
        return;
    }
    KESTREL_CRITICAL
    {
        kestrel_current->wake_tick = kestrel_get_ticks() + ticks;
        kestrel_current->state = KESTREL_TASK_SLEEPING;
        kestrel_yield();
    }
}

void kestrel_wake_sleeping(uint32_t now)
{
    for (uint8_t i = 0; i < task_count; i++) {
        kestrel_tcb_t *tcb = &tcbs[i];
        if (tcb->state == KESTREL_TASK_SLEEPING && (int32_t)(now - tcb->wake_tick) >= 0) {
            tcb->state = KESTREL_TASK_READY;
        }
    }
}

void kestrel_delay_until(uint32_t *last_wake, uint32_t period)
{
    if (last_wake == NULLPTR || period == 0) {
        return;
    }
    KESTREL_CRITICAL
    {
        uint32_t now = kestrel_get_ticks();
        uint32_t target_time = *last_wake + period;
        *last_wake = target_time;
        if ((int32_t)(now - *last_wake) < 0) {
            kestrel_current->wake_tick = target_time;
            kestrel_current->state = KESTREL_TASK_SLEEPING;
            kestrel_yield();
        }
    }
}
