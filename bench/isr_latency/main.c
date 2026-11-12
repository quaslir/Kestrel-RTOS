/*
 * Interrupt -> waiting task latency, CPU 100% busy.
 *
 * TIM2 fires at 100 Hz. The ISR stores DWT->CYCCNT and gives a semaphore; a
 * priority-3 task waiting on it records the cycles from that store to the
 * point where kestrel_sem_take() returns. Two priority-1 busy loops keep the
 * CPU fully loaded, so every sample includes a real preemption.
 *
 * The first interrupt also checks that blocking calls from an ISR are refused
 * with KESTREL_ERR_IN_ISR.
 */
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "bench.h"
#include "uart.h"

#include "kestrel/critical.h"
#include "kestrel/mutex.h"
#include "kestrel/semaphore.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#define PRIO_BACKGROUND 1U
#define PRIO_REPORTER   2U
#define PRIO_HANDLER    3U

#define TIM2_IRQ_PRIO 6U
#define TIM2_PSC      (72U - 1U)    /* 1 MHz timer clock */
#define TIM2_ARR      (10000U - 1U) /* 100 Hz */

static kestrel_sem_t irq_sem;
static kestrel_sem_t empty_sem;
static kestrel_mutex_t isr_mutex;

static volatile uint32_t isr_time;
static volatile uint32_t irq_count;
static volatile kestrel_status_t isr_take_status = KESTREL_OK;
static volatile kestrel_status_t isr_lock_status = KESTREL_OK;
static volatile bench_stat_t window;
static volatile uint32_t bg_counter[2];

static void tim2_start(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    TIM2->PSC = TIM2_PSC;
    TIM2->ARR = TIM2_ARR;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->DIER = TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, TIM2_IRQ_PRIO);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 = TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF;
    irq_count++;

    if (irq_count == 1U) {
        isr_take_status = kestrel_sem_take(&empty_sem, 10U);
        isr_lock_status = kestrel_mutex_lock(&isr_mutex, KESTREL_NO_WAIT);
    }

    isr_time = DWT->CYCCNT;
    (void)kestrel_sem_give(&irq_sem);
}

static void handler_task(void *arg)
{
    (void)arg;
    tim2_start();

    while (1) {
        if (kestrel_sem_take(&irq_sem, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            bench_fail();
        }
        uint32_t lat = DWT->CYCCNT - isr_time;
        bench_stat_add(&window, lat);
    }
}

static void background_task(void *arg)
{
    volatile uint32_t *counter = (volatile uint32_t *)arg;

    while (1) {
        (*counter)++;
    }
}

static void put_status(const char *name, kestrel_status_t status)
{
    uart_puts(name);
    uart_puts(status == KESTREL_ERR_IN_ISR ? "=IN_ISR" : "=WRONG");
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    uint32_t prev_bg = 0U;
    bench_stat_t total;
    bench_stat_reset(&total);

    while (1) {
        kestrel_delay_until(&last, BENCH_REPORT_PERIOD_TICKS);

        bench_stat_t w;
        uint32_t bg;
        KESTREL_CRITICAL
        {
            w = window;
            bench_stat_reset(&window);
            bg = bg_counter[0] + bg_counter[1];
        }
        if (w.count == 0U) {
            continue;
        }
        bench_stat_merge(&total, &w);

        uart_puts("isr t=");
        uart_put_u32(last / 1000U);
        uart_puts("s n=");
        uart_put_u32(w.count);
        uart_puts(" min=");
        uart_put_u32(w.min);
        uart_puts(" avg=");
        uart_put_u32(bench_stat_avg(&w));
        uart_puts(" max=");
        uart_put_u32(w.max);
        uart_puts(" bg/s=");
        uart_put_u32(bg - prev_bg);
        uart_puts(" | total min=");
        uart_put_u32(total.min);
        uart_puts(" max=");
        uart_put_u32(total.max);
        uart_puts(" | ");
        put_status("take", isr_take_status);
        uart_puts(" ");
        put_status("lock", isr_lock_status);
        uart_puts("\r\n");

        prev_bg = bg;
    }
}

int main(void)
{
    bench_board_init("bench_isr_latency");
    bench_stat_reset(&window);

    if (kestrel_sem_init(&irq_sem, 0U, 1U) != KESTREL_OK ||
        kestrel_sem_init(&empty_sem, 0U, 1U) != KESTREL_OK ||
        kestrel_mutex_init(&isr_mutex) != KESTREL_OK) {
        bench_fail();
    }

    bench_create_task(background_task, (void *)&bg_counter[0], PRIO_BACKGROUND);
    bench_create_task(background_task, (void *)&bg_counter[1], PRIO_BACKGROUND);
    bench_create_task(reporter_task, NULL, PRIO_REPORTER);
    bench_create_task(handler_task, NULL, PRIO_HANDLER);
    kestrel_start();
}
