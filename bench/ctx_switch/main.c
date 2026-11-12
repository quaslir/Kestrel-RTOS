/*
 * Context switch time, task -> task.
 *
 * Two tasks of equal priority ping-pong with kestrel_yield(). Each one stores
 * DWT->CYCCNT just before yielding; the other one reads it right after it
 * resumes. A sample is kept only if no tick happened in between, so SysTick
 * never inflates the result. USE_FPU=1 makes both tasks touch the FPU once,
 * so every switch also saves and restores S16-S31.
 */
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "bench.h"
#include "uart.h"

#include "kestrel/critical.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#ifndef USE_FPU
#error "Define USE_FPU as 0 or 1"
#endif

#define PRIO_PINGPONG 2U
#define PRIO_REPORTER 3U

typedef struct {
    bench_stat_t cycles;
    uint32_t disturbed;
} window_t;

static volatile uint32_t switch_start;
static volatile uint32_t switch_tick;
static volatile window_t window;

static void mark_and_yield(void)
{
    switch_tick = kestrel_get_ticks();
    switch_start = DWT->CYCCNT;
    kestrel_yield();
}

static void pingpong_task(void *arg)
{
    (void)arg;
#if USE_FPU
    // clang-format off
    __asm volatile("vmov s0, %0" : : "r"(1U) : "s0");
    // clang-format on
#endif
    mark_and_yield();

    while (1) {
        uint32_t tick_before = kestrel_get_ticks();
        uint32_t now = DWT->CYCCNT;
        uint32_t start = switch_start;
        uint32_t start_tick = switch_tick;
        uint32_t now_tick = kestrel_get_ticks();

        /* The reporter has a higher priority: update the window atomically
         * so it never snapshots or resets it half-way through. */
        KESTREL_CRITICAL
        {
            if (tick_before == start_tick && now_tick == start_tick) {
                bench_stat_add(&window.cycles, now - start);
            } else {
                window.disturbed++;
            }
        }

        mark_and_yield();
    }
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    bench_stat_t total;
    bench_stat_reset(&total);

    while (1) {
        kestrel_delay_until(&last, BENCH_REPORT_PERIOD_TICKS);

        window_t w;
        KESTREL_CRITICAL
        {
            w = window;
            bench_stat_reset(&window.cycles);
            window.disturbed = 0U;
        }
        if (w.cycles.count == 0U) {
            continue;
        }
        bench_stat_merge(&total, &w.cycles);

        uart_puts(USE_FPU ? "fpu" : "int");
        uart_puts(" t=");
        uart_put_u32(last / 1000U);
        uart_puts("s n=");
        uart_put_u32(w.cycles.count);
        uart_puts(" dist=");
        uart_put_u32(w.disturbed);
        uart_puts(" min=");
        uart_put_u32(w.cycles.min);
        uart_puts(" avg=");
        uart_put_u32(bench_stat_avg(&w.cycles));
        uart_puts(" max=");
        uart_put_u32(w.cycles.max);
        uart_puts(" | total min=");
        uart_put_u32(total.min);
        uart_puts(" avg=");
        uart_put_u32(bench_stat_avg(&total));
        uart_puts(" max=");
        uart_put_u32(total.max);
        uart_puts("\r\n");
    }
}

int main(void)
{
    bench_board_init(USE_FPU ? "bench_ctx_switch_fpu" : "bench_ctx_switch");
    bench_stat_reset(&window.cycles);

    bench_create_task(pingpong_task, NULL, PRIO_PINGPONG);
    bench_create_task(pingpong_task, NULL, PRIO_PINGPONG);
    bench_create_task(reporter_task, NULL, PRIO_REPORTER);
    kestrel_start();
}
