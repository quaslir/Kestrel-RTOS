# Kestrel RTOS — API reference

Public API for applications. Everything here is declared in `kernel/include/kestrel/`.
Kernel internals (`kestrel_internal.h`, `ready.h`, `wait_list.h`) are private and cannot be included by the application.

**Conventions**
- Priorities: `0` (lowest) … `KESTREL_MAX_PRIORITY` (`31`, highest). A larger number means more important.
- Time is measured in **ticks** (1 tick = 1 ms at the default 1 kHz SysTick).
- Functions that can fail return `kestrel_status_t`. `KESTREL_OK` is `0`, errors are negative.
- **Blocks** — the call may put the calling task to sleep.
- **ISR** — the call may be used from an interrupt handler (see [Interrupts](#interrupts)).

---

## Contents
- [Constants](#constants)
- [Status codes](#status-codes)
- [Tasks](#tasks) — `task.h`
- [Time](#time) — `time.h`
- [Critical sections](#critical-sections) — `critical.h`
- [Semaphores](#semaphores) — `semaphore.h`
- [Mutexes](#mutexes) — `mutex.h`
- [Message queues](#message-queues) — `queue.h`
- [Interrupts](#interrupts)
- [Build options](#build-options)

---

## Constants

| Name | Value | Meaning |
|---|---|---|
| `MAX_TASKS` | `8` | Maximum number of application tasks (the idle task is extra) |
| `STACK_SIZE` | `256` | Stack size per task, in 32-bit words (1 KB) |
| `KESTREL_MAX_PRIORITY` | `31` | Highest priority |
| `KESTREL_NO_WAIT` | `0` | Timeout: do not block, fail immediately if the resource is not available |
| `KESTREL_WAIT_FOREVER` | `0xFFFFFFFF` | Timeout: block until the resource becomes available |

---

## Status codes

`kestrel_status_t`

| Code | Value | Returned when |
|---|---|---|
| `KESTREL_OK` | `0` | Success |
| `KESTREL_ERR_NO_SLOT` | `-1` | All `MAX_TASKS` task slots are used |
| `KESTREL_ERR_NULL_FN` | `-2` | Task function is `NULL` |
| `KESTREL_ERR_INVALID_PRIORITY` | `-3` | Priority is greater than `KESTREL_MAX_PRIORITY` |
| `KESTREL_ERR_TIMEOUT` | `-4` | The timeout expired, or `KESTREL_NO_WAIT` was given and the resource was not available |
| `KESTREL_ERR_OVERFLOW` | `-5` | Semaphore is already at its maximum count |
| `KESTREL_ERR_INVALID_ARG` | `-6` | `NULL` pointer or invalid parameter; mutex locked twice by the same task |
| `KESTREL_ERR_NOT_OWNER` | `-7` | Mutex unlocked by a task that does not own it |
| `KESTREL_ERR_IN_ISR` | `-8` | A call that would block was made from an interrupt handler |

---

## Tasks

`#include "kestrel/task.h"`

```c
typedef void (*kestrel_task_fn_t)(void *arg);

typedef struct {
    kestrel_status_t status;
    uint8_t id;
} kestrel_task_result_t;
```

### `kestrel_new_task`
```c
kestrel_task_result_t kestrel_new_task(kestrel_task_fn_t task, void *data, uint8_t priority);
```
Creates a task. Its stack and control block are allocated statically. The task starts running only after `kestrel_start()`.

| Parameter | |
|---|---|
| `task` | Task function. It may loop forever or return; a task that returns is removed from scheduling |
| `data` | Passed to the task function as `arg` |
| `priority` | `0` … `KESTREL_MAX_PRIORITY` |

**Returns:** `status` — `KESTREL_OK`, `KESTREL_ERR_NO_SLOT`, `KESTREL_ERR_NULL_FN` or `KESTREL_ERR_INVALID_PRIORITY`; `id` — the task's slot number (valid only on success).
Call before `kestrel_start()`.

### `kestrel_start`
```c
__attribute__((noreturn)) void kestrel_start(void);
```
Creates the idle task, configures the exception priorities and starts the highest-priority ready task. Never returns. At least one task must have been created.

### `kestrel_yield`
```c
void kestrel_yield(void);
```
Gives up the CPU to the next ready task of the **same** priority (round-robin). If there is none, the calling task continues.

### `kestrel_delay`
```c
void kestrel_delay(uint32_t ticks);
```
**Blocks.** Sleeps for `ticks` ticks counted from the moment of the call. Work done between calls accumulates — use `kestrel_delay_until` for periodic tasks.

### `kestrel_delay_until`
```c
bool kestrel_delay_until(uint32_t *last_wake, uint32_t period);
```
**Blocks.** Sleeps until `*last_wake + period`, then advances `*last_wake` by `period`. Keeps a fixed period with no drift.

| Parameter | |
|---|---|
| `last_wake` | Wake-up time of the previous period. Initialise once with `kestrel_get_ticks()` before the loop |
| `period` | Period in ticks |

**Returns:** `true` if the task slept; `false` if the deadline had already passed (a missed period).

```c
uint32_t last = kestrel_get_ticks();
while (1) {
    if (!kestrel_delay_until(&last, 1)) {
        missed++;
    }
    /* runs every 1 ms */
}
```

---

## Time

`#include "kestrel/time.h"`

### `kestrel_get_ticks`
```c
uint32_t kestrel_get_ticks(void);
```
**ISR.** Number of ticks since start-up. Wraps around after ~49.7 days at 1 kHz; compare times by subtraction (`now - start >= duration`), never with `<` on absolute values.

### `kestrel_tick`
```c
bool kestrel_tick(void);
```
Called by the port's `SysTick_Handler`, **not by applications**. Advances the tick counter, wakes sleeping tasks and timed-out waiters, and returns `true` if a context switch is needed.

---

## Critical sections

`#include "kestrel/critical.h"`

### `KESTREL_CRITICAL`
```c
KESTREL_CRITICAL {
    /* short code that must not be interrupted by the kernel */
}
```
**ISR.** Raises `BASEPRI` to `PORT_MAX_SYSCALL_PRIO` for the duration of the block: the tick, PendSV and all interrupts that may call the kernel are held off; more urgent interrupts still run. Nestable.

**Rules**
- Keep it to microseconds. While inside, the tick cannot advance; a second tick in the same window is lost.
- **Never use `return`, `break` or `goto` to leave the block** — the previous `BASEPRI` would not be restored and the system would stop scheduling.
- Do not call blocking functions inside.

---

## Semaphores

`#include "kestrel/semaphore.h"`

```c
typedef struct { /* count, max, waiters */ } kestrel_sem_t;
```
Counting semaphore. With `max = 1` it is a binary semaphore. Waiting tasks are served highest priority first, FIFO among equal priorities. `give` hands the unit directly to the waiting task.

### `kestrel_sem_init`
```c
kestrel_status_t kestrel_sem_init(kestrel_sem_t *sem, uint32_t initial, uint32_t max);
```
| Parameter | |
|---|---|
| `initial` | Starting count, `0` … `max` |
| `max` | Maximum count, at least `1` |

**Returns:** `KESTREL_OK`, or `KESTREL_ERR_INVALID_ARG` (`sem == NULL`, `max == 0`, `initial > max`).

### `kestrel_sem_take`
```c
kestrel_status_t kestrel_sem_take(kestrel_sem_t *sem, uint32_t timeout);
```
**Blocks** (unless `timeout == KESTREL_NO_WAIT`). **ISR** only with `KESTREL_NO_WAIT`.
Takes one unit. If the count is zero, waits up to `timeout` ticks.

**Returns:** `KESTREL_OK`, `KESTREL_ERR_TIMEOUT`, `KESTREL_ERR_INVALID_ARG`, `KESTREL_ERR_IN_ISR`.

### `kestrel_sem_give`
```c
kestrel_status_t kestrel_sem_give(kestrel_sem_t *sem);
```
**ISR.** Never blocks. If a task is waiting, it is woken and receives the unit; if it is more important than the current task, the switch happens immediately (from an ISR: on ISR exit). Otherwise the count is increased.

**Returns:** `KESTREL_OK`, `KESTREL_ERR_OVERFLOW` (count already at `max`), `KESTREL_ERR_INVALID_ARG`.

---

## Mutexes

`#include "kestrel/mutex.h"`

```c
typedef struct { /* owner, waiters */ } kestrel_mutex_t;
```
Mutual exclusion with an owner and **priority inheritance**: while a more important task waits, the owner runs at the waiter's priority. Not recursive. **Never usable from an ISR.**

### `kestrel_mutex_init`
```c
kestrel_status_t kestrel_mutex_init(kestrel_mutex_t *mutex);
```
**Returns:** `KESTREL_OK`, `KESTREL_ERR_INVALID_ARG`.

### `kestrel_mutex_lock`
```c
kestrel_status_t kestrel_mutex_lock(kestrel_mutex_t *mutex, uint32_t timeout);
```
**Blocks** (unless `timeout == KESTREL_NO_WAIT`). Takes ownership. If the mutex is owned by another task, waits up to `timeout` ticks and raises the owner's priority to the caller's if it is lower.

**Returns:** `KESTREL_OK`, `KESTREL_ERR_TIMEOUT`, `KESTREL_ERR_INVALID_ARG` (`NULL`, or already owned by the caller), `KESTREL_ERR_IN_ISR`.

### `kestrel_mutex_unlock`
```c
kestrel_status_t kestrel_mutex_unlock(kestrel_mutex_t *mutex);
```
Releases ownership and restores the caller's base priority. If tasks are waiting, ownership is handed directly to the most important one.

**Returns:** `KESTREL_OK`, `KESTREL_ERR_NOT_OWNER`, `KESTREL_ERR_INVALID_ARG`, `KESTREL_ERR_IN_ISR`.

**Limitations:** see *Known limitations* in the README (single-level inheritance, several mutexes held at once, waiter timeout, task returning while holding a mutex).

---

## Message queues

`#include "kestrel/queue.h"`

```c
typedef struct { /* storage, item_size, capacity, head, tail, count, receivers, senders */ } kestrel_queue_t;
```
FIFO of fixed-size items, copied by value. Memory is supplied by the application. If a receiver is waiting, `send` copies the item straight into its buffer; if a sender is waiting on a full queue, `receive` moves its item into the freed slot.

Items are copied inside a critical section — keep them small and pass pointers for large data.

### `kestrel_queue_init`
```c
kestrel_status_t kestrel_queue_init(kestrel_queue_t *q, void *storage,
                                    uint32_t item_size, uint32_t capacity);
```
| Parameter | |
|---|---|
| `storage` | Buffer of at least `item_size * capacity` bytes, alive as long as the queue |
| `item_size` | Size of one item in bytes, at least `1` |
| `capacity` | Number of items, at least `1` |

**Returns:** `KESTREL_OK`, `KESTREL_ERR_INVALID_ARG`.

```c
static my_item_t storage[8];
static kestrel_queue_t q;
kestrel_queue_init(&q, storage, sizeof(my_item_t), 8);
```

### `kestrel_queue_send`
```c
kestrel_status_t kestrel_queue_send(kestrel_queue_t *q, const void *item, uint32_t timeout);
```
**Blocks** (unless `timeout == KESTREL_NO_WAIT`). **ISR** only with `KESTREL_NO_WAIT`.
Copies `item_size` bytes from `item` into the queue. If the queue is full, waits up to `timeout` ticks for space.

**Returns:** `KESTREL_OK`, `KESTREL_ERR_TIMEOUT`, `KESTREL_ERR_INVALID_ARG`, `KESTREL_ERR_IN_ISR`.

### `kestrel_queue_receive`
```c
kestrel_status_t kestrel_queue_receive(kestrel_queue_t *q, void *buffer, uint32_t timeout);
```
**Blocks** (unless `timeout == KESTREL_NO_WAIT`). **ISR** only with `KESTREL_NO_WAIT`.
Copies the oldest item into `buffer`. If the queue is empty, waits up to `timeout` ticks.

**Returns:** `KESTREL_OK`, `KESTREL_ERR_TIMEOUT`, `KESTREL_ERR_INVALID_ARG`, `KESTREL_ERR_IN_ISR`.

---

## Interrupts

| Function | From an ISR |
|---|---|
| `kestrel_sem_give` | ✅ |
| `kestrel_queue_send`, `kestrel_queue_receive` | ✅ with `KESTREL_NO_WAIT` only |
| `kestrel_sem_take` | ✅ with `KESTREL_NO_WAIT` only |
| `kestrel_get_ticks`, `KESTREL_CRITICAL` | ✅ |
| `kestrel_mutex_lock`, `kestrel_mutex_unlock` | ❌ always `KESTREL_ERR_IN_ISR` |
| `kestrel_delay`, `kestrel_delay_until`, `kestrel_yield` | ❌ never call |

**Priority rule.** An interrupt that calls the kernel must have a priority numerically **≥ `PORT_MAX_SYSCALL_PRIO` (`0x50`)**, i.e. `NVIC_SetPriority(irq, 5)` or a larger number. More urgent interrupts are never masked by the kernel and must not call it.

```c
NVIC_SetPriority(TIM2_IRQn, 6);   /* may call the kernel */
NVIC_EnableIRQ(TIM2_IRQn);

void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;
    kestrel_sem_give(&irq_sem);   /* the waiting task runs right after the ISR returns */
}
```

A call that would block from an ISR returns `KESTREL_ERR_IN_ISR` instead of putting the interrupted task to sleep. For semaphores and queues this is detected only when the call would actually block.

---

## Build options

| CMake option | Default | Effect |
|---|---|---|
| `KESTREL_PRIORITY_INHERITANCE` | `ON` | Priority inheritance in mutexes (`OFF` reproduces the priority-inversion demo) |
| `CMAKE_BUILD_TYPE` | — | `Release` for all published measurements |
