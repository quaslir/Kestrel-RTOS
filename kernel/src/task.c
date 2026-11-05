#include "kestrel/task.h"
#include "port.h"
#include <stdbool.h>
#include <stddef.h>
#include "kestrel/critical.h"
#include "kestrel/time.h"
#include "kestrel_internal.h"
#include "ready.h"
#define IDLE_STACK_SIZE 64
kestrel_tcb_t *volatile kestrel_current = NULL;
volatile bool kestrel_started = false;
static uint8_t task_count = 0;
__attribute__((aligned(8))) static uint32_t stacks[MAX_TASKS][STACK_SIZE];
static kestrel_tcb_t tcbs[MAX_TASKS];

__attribute__((aligned(8))) static uint32_t idle_stack[IDLE_STACK_SIZE];
static kestrel_tcb_t idle_tcb;

__attribute__((noreturn)) static void kernel_halt(void)
{
    while (1) {
        __asm volatile("wfi");
    }
}
__attribute__((noreturn)) static void idle_task(void *arg)
{
    (void)arg;
    while (1) {
        __asm volatile("wfi");
    }
}

static void write_null(uint32_t **preg, uint8_t times)
{
    uint32_t *reg = *preg;
    for (uint8_t i = 0; i < times; i++) {
        *--reg = 0U;
    }

    *preg = reg;
}

static void make_ready(kestrel_tcb_t *tcb)
{
    tcb->state = KESTREL_TASK_READY;
    ready_insert(tcb);
}

static void block_current(kestrel_task_state_t state)
{
    kestrel_current->state = state;
    ready_remove(kestrel_current);
    kestrel_yield();
}

__attribute__((noreturn)) static void task_exit(void)
{
    KESTREL_CRITICAL
    {
        block_current(KESTREL_TASK_DEAD);
    }
    while (1) {
        __asm volatile("wfi");
    }
}
static uint32_t *build_frame(uint32_t *stack_end, kestrel_task_fn_t fn, void *arg)
{
    uint32_t *sp = stack_end;
    *--sp = INITIAL_XPSR;
    *--sp = (uint32_t)fn & CLEAR_BIT_ZERO;
    *--sp = (uint32_t)&task_exit;

    write_null(&sp, 4);

    *--sp = (uint32_t)arg;
    write_null(&sp, 8);
    return sp;
}

kestrel_task_result_t kestrel_new_task(kestrel_task_fn_t task, void *data, uint8_t priority)
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

    if (priority > KESTREL_MAX_PRIORITY) {
        result.status = KESTREL_ERR_INVALID_PRIORITY;
        return result;
    }

    uint32_t *sp = build_frame(&stacks[task_count][STACK_SIZE], task, data);

    result.status = KESTREL_OK;
    result.id = task_count;
    tcbs[task_count].sp = sp;
    tcbs[task_count].priority = priority;
    make_ready(&tcbs[task_count++]);
    return result;
}

void kestrel_start(void)
{
    if (task_count == 0) {
        kernel_halt();
    }

    idle_tcb.sp = build_frame(idle_stack + IDLE_STACK_SIZE, &idle_task, NULL);
    kestrel_current = &idle_tcb;
    kestrel_schedule();
    port_init();
    idle_tcb.state = KESTREL_TASK_READY;
    __asm volatile("svc 0"); // enter the interrupt handler mode

    kernel_halt();
}

void kestrel_schedule(void)
{
    if (task_count == 0) {
        return;
    }
    kestrel_tcb_t *next_task = ready_pick();
    if (!next_task) {
        kestrel_current = &idle_tcb;
    } else {
        kestrel_current = next_task;
    }
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
        block_current(KESTREL_TASK_SLEEPING);
    }
}

void kestrel_wake_sleeping(uint32_t now)
{
    for (uint8_t i = 0; i < task_count; i++) {
        kestrel_tcb_t *tcb = &tcbs[i];
        if (tcb->state == KESTREL_TASK_SLEEPING && (int32_t)(now - tcb->wake_tick) >= 0) {
            make_ready(tcb);
        }
    }
}

bool kestrel_delay_until(uint32_t *last_wake, uint32_t period)
{
    if (last_wake == NULL || period == 0 || !kestrel_current) {
        return false;
    }

    bool delayed = false;

    KESTREL_CRITICAL
    {
        uint32_t now = kestrel_get_ticks();
        uint32_t target_time = *last_wake + period;
        *last_wake = target_time;
        if ((int32_t)(now - target_time) < 0) {
            kestrel_current->wake_tick = target_time;
            block_current(KESTREL_TASK_SLEEPING);
            delayed = true;
        }
    }

    return delayed;
}
