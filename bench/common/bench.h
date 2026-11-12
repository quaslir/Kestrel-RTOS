#ifndef KESTREL_BENCH_H
#define KESTREL_BENCH_H

/*
 * Helpers shared by the benchmark firmware: board start-up, the DWT cycle
 * counter, decimal UART output and a min/avg/max accumulator.
 */

#include <stdint.h>

#include "stm32f3xx.h"

#include "kestrel/task.h"

#define BENCH_CPU_MHZ             72U
#define BENCH_REPORT_PERIOD_TICKS 1000U

typedef struct {
    uint32_t count;
    uint32_t min;
    uint32_t max;
    uint64_t sum;
} bench_stat_t;

/* Clock 72 MHz, SysTick 1 kHz, DWT, LED, UART 115200; prints the banner. */
void bench_board_init(const char *name);

/* Turns the LED on and stops. Used for setup errors only. */
__attribute__((noreturn)) void bench_fail(void);

void bench_create_task(kestrel_task_fn_t fn, void *arg, uint8_t priority);

void uart_put_u32(uint32_t value);

static inline void bench_stat_reset(volatile bench_stat_t *s)
{
    s->count = 0U;
    s->min = 0xFFFFFFFFU;
    s->max = 0U;
    s->sum = 0U;
}

static inline void bench_stat_add(volatile bench_stat_t *s, uint32_t value)
{
    s->count++;
    s->sum += value;
    if (value < s->min) {
        s->min = value;
    }
    if (value > s->max) {
        s->max = value;
    }
}

static inline uint32_t bench_stat_avg(const bench_stat_t *s)
{
    return s->count ? (uint32_t)(s->sum / s->count) : 0U;
}

/* Folds a window snapshot into the totals kept since reset. */
static inline void bench_stat_merge(bench_stat_t *total, const bench_stat_t *window)
{
    total->count += window->count;
    total->sum += window->sum;
    if (window->min < total->min) {
        total->min = window->min;
    }
    if (window->max > total->max) {
        total->max = window->max;
    }
}

#endif
