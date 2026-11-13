/*
 * Wake-up latency and period jitter of a 1 ms periodic task.
 *
 * A priority-5 task runs every tick with kestrel_delay_until(). Right after it
 * wakes it reads how far SysTick has counted since the tick (= wake-up
 * latency) and the DWT cycles since its previous wake-up (= period). It then
 * busy-works 14 400 cycles (0.2 ms) and counts a deadline miss if the tick
 * changed meanwhile.
 *
 * LOAD_ENABLED=1 adds priority-1 load that keeps the CPU 100% busy and
 * exercises every kernel path: a producer/consumer pair on a queue, two tasks
 * contending for one mutex, and a 10 kHz TIM2 interrupt giving a semaphore.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "bench.h"
#include "uart.h"

#include "kestrel/critical.h"
#include "kestrel/mutex.h"
#include "kestrel/queue.h"
#include "kestrel/semaphore.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#ifndef LOAD_ENABLED
#error "Define LOAD_ENABLED as 0 or 1"
#endif

#define PRIO_LOAD     1U
#define PRIO_REPORTER 2U
#define PRIO_PERIODIC 5U

#define WORK_CYCLES 14400U

#define TIM2_IRQ_PRIO 6U
#define TIM2_PSC      (72U - 1U)  /* 1 MHz timer clock */
#define TIM2_ARR      (100U - 1U) /* 10 kHz */

#define QUEUE_CAPACITY 8U
#define MUTEX_SPIN     50U

/* 16 bytes: the queue copies items inside a critical section, so the item
 * size bounds the worst-case latency of the periodic task. */
typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
} item_t;

static volatile bench_stat_t lat_win;
static volatile bench_stat_t per_win;
static volatile uint32_t missed;
static volatile uint32_t deadline_miss;

static volatile uint32_t q_count;
static volatile uint32_t mtx_count;
static volatile uint32_t isr_count;
static volatile uint32_t isr_taken;

static item_t queue_storage[QUEUE_CAPACITY];
static kestrel_queue_t q;
static kestrel_mutex_t mtx;
static kestrel_sem_t isr_sem;

static void periodic_task(void *arg)
{
    (void)arg;
    uint32_t scale = (SysTick->CTRL & SysTick_CTRL_CLKSOURCE_Msk) ? 1U : 8U;
    uint32_t last = kestrel_get_ticks();
    uint32_t prev = 0U;
    bool first = true;

    while (1) {
        bool slept = kestrel_delay_until(&last, 1U);
        uint32_t lat = (SysTick->LOAD - SysTick->VAL) * scale;
        uint32_t now = DWT->CYCCNT;

        if (!slept) {
            missed++;
        }
        bench_stat_add(&lat_win, lat);
        if (!first) {
            bench_stat_add(&per_win, now - prev);
        }
        prev = now;
        first = false;

        while ((DWT->CYCCNT - now) < WORK_CYCLES) {
        }
        if (kestrel_get_ticks() != last) {
            deadline_miss++;
        }
    }
}

#if LOAD_ENABLED
static void producer_task(void *arg)
{
    (void)arg;
    item_t item = { 0U, 1U, 2U, 3U };

    while (1) {
        item.a++;
        if (kestrel_queue_send(&q, &item, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            bench_fail();
        }
    }
}

static void consumer_task(void *arg)
{
    (void)arg;
    item_t item;

    while (1) {
        if (kestrel_queue_receive(&q, &item, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            bench_fail();
        }
        q_count++;
    }
}

static void mutex_task(void *arg)
{
    (void)arg;

    while (1) {
        if (kestrel_mutex_lock(&mtx, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            bench_fail();
        }
        for (volatile uint32_t i = 0U; i < MUTEX_SPIN; i++) {
        }
        mtx_count++;
        if (kestrel_mutex_unlock(&mtx) != KESTREL_OK) {
            bench_fail();
        }
    }
}

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
    isr_count++;
    (void)kestrel_sem_give(&isr_sem);
}

static void isr_consumer_task(void *arg)
{
    (void)arg;
    tim2_start();

    while (1) {
        if (kestrel_sem_take(&isr_sem, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            bench_fail();
        }
        isr_taken++;
    }
}
#endif

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    uint32_t prev_q = 0U;
    uint32_t prev_mtx = 0U;
    uint32_t prev_isr = 0U;
    uint32_t prev_taken = 0U;
    bench_stat_t total_lat;
    bench_stat_t total_per;
    bench_stat_reset(&total_lat);
    bench_stat_reset(&total_per);

    while (1) {
        kestrel_delay_until(&last, BENCH_REPORT_PERIOD_TICKS);

        bench_stat_t lat;
        bench_stat_t per;
        uint32_t qn, mn, in, tn, miss, dl;
        KESTREL_CRITICAL {
            lat = lat_win;
            per = per_win;
            bench_stat_reset(&lat_win);
            bench_stat_reset(&per_win);
            qn = q_count;
            mn = mtx_count;
            in = isr_count;
            tn = isr_taken;
            miss = missed;
            dl = deadline_miss;
        }
        if (lat.count == 0U || per.count == 0U) {
            continue;
        }
        bench_stat_merge(&total_lat, &lat);
        bench_stat_merge(&total_per, &per);

        uart_puts(LOAD_ENABLED ? "load" : "idle");
        uart_puts(" t=");
        uart_put_u32(last / 1000U);
        uart_puts("s lat min=");
        uart_put_u32(lat.min);
        uart_puts(" avg=");
        uart_put_u32(bench_stat_avg(&lat));
        uart_puts(" max=");
        uart_put_u32(lat.max);
        uart_puts(" per min=");
        uart_put_u32(per.min);
        uart_puts(" max=");
        uart_put_u32(per.max);
        uart_puts(" missed=");
        uart_put_u32(miss);
        uart_puts(" dl=");
        uart_put_u32(dl);
        uart_puts(" | q/s=");
        uart_put_u32(qn - prev_q);
        uart_puts(" mtx/s=");
        uart_put_u32(mn - prev_mtx);
        uart_puts(" isr/s=");
        uart_put_u32(in - prev_isr);
        uart_puts(" took/s=");
        uart_put_u32(tn - prev_taken);
        uart_puts(" | total lat=");
        uart_put_u32(total_lat.min);
        uart_puts("/");
        uart_put_u32(bench_stat_avg(&total_lat));
        uart_puts("/");
        uart_put_u32(total_lat.max);
        uart_puts(" (max ");
        uart_put_u32(total_lat.max / BENCH_CPU_MHZ);
        uart_puts("us) per=");
        uart_put_u32(total_per.min);
        uart_puts("..");
        uart_put_u32(total_per.max);
        uart_puts("\r\n");

        prev_q = qn;
        prev_mtx = mn;
        prev_isr = in;
        prev_taken = tn;
    }
}

int main(void)
{
    bench_board_init(LOAD_ENABLED ? "bench_jitter" : "bench_jitter_idle");
    bench_stat_reset(&lat_win);
    bench_stat_reset(&per_win);

    if (kestrel_queue_init(&q, queue_storage, sizeof(item_t), QUEUE_CAPACITY) != KESTREL_OK ||
        kestrel_mutex_init(&mtx) != KESTREL_OK ||
        kestrel_sem_init(&isr_sem, 0U, 1U) != KESTREL_OK) {
        bench_fail();
    }

    bench_create_task(periodic_task, NULL, PRIO_PERIODIC);
    bench_create_task(reporter_task, NULL, PRIO_REPORTER);
#if LOAD_ENABLED
    bench_create_task(producer_task, NULL, PRIO_LOAD);
    bench_create_task(consumer_task, NULL, PRIO_LOAD);
    bench_create_task(mutex_task, NULL, PRIO_LOAD);
    bench_create_task(mutex_task, NULL, PRIO_LOAD);
    bench_create_task(isr_consumer_task, NULL, PRIO_LOAD);
#endif
    kestrel_start();
}
