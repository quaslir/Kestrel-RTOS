#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/critical.h"
#include "kestrel/mutex.h"
#include "kestrel/queue.h"
#include "kestrel/semaphore.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define LOAD_ENABLED 0

#define PRIO_LOAD     1U
#define PRIO_REPORTER 2U
#define PRIO_PERIODIC 5U

#define CPU_HZ              72000000U
#define CPU_MHZ             72U
#define WORK_CYCLES         14400U
#define REPORT_PERIOD_TICKS 1000U

#define TIM2_IRQ_PRIO 6U
#define TIM2_PSC      (72U - 1U)
#define TIM2_ARR      (100U - 1U)

#define QUEUE_CAPACITY 8U
#define MUTEX_SPIN     50U

typedef struct {
    uint32_t count;
    uint32_t min;
    uint32_t max;
    uint64_t sum;
} stat_t;

typedef struct {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
} item_t;

static volatile stat_t lat_win;
static volatile stat_t per_win;
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

static void uart_put_u32(uint32_t value)
{
    char buf[10];
    uint8_t len = 0;

    do {
        buf[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    while (len > 0U) {
        uart_putc(buf[--len]);
    }
}

static void output_pin_init(GPIO_TypeDef *port, uint8_t pin)
{
    gpio_enable_port(port);
    gpio_set_mode(port, pin, GPIO_MODE_OUTPUT);
    gpio_pull(port, pin, GPIO_PULL_NONE);
    gpio_put(port, pin, false);
}

static void fail(void)
{
    output_pin_init(LED_PORT, LED_PIN);
    gpio_put(LED_PORT, LED_PIN, true);
    while (1) {
    }
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void stat_reset(volatile stat_t *s)
{
    s->count = 0U;
    s->min = 0xFFFFFFFFU;
    s->max = 0U;
    s->sum = 0U;
}

static void stat_add(volatile stat_t *s, uint32_t v)
{
    s->count++;
    s->sum += v;
    if (v < s->min) {
        s->min = v;
    }
    if (v > s->max) {
        s->max = v;
    }
}

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
        stat_add(&lat_win, lat);
        if (!first) {
            stat_add(&per_win, now - prev);
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

static void producer_task(void *arg)
{
    (void)arg;
    item_t item = { 0U, 1U, 2U, 3U };

    while (1) {
        item.a++;
        if (kestrel_queue_send(&q, &item, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            fail();
        }
    }
}

static void consumer_task(void *arg)
{
    (void)arg;
    item_t item;

    while (1) {
        if (kestrel_queue_receive(&q, &item, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            fail();
        }
        q_count++;
    }
}

static void mutex_task(void *arg)
{
    (void)arg;

    while (1) {
        if (kestrel_mutex_lock(&mtx, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            fail();
        }
        for (volatile uint32_t i = 0U; i < MUTEX_SPIN; i++) {
        }
        mtx_count++;
        if (kestrel_mutex_unlock(&mtx) != KESTREL_OK) {
            fail();
        }
    }
}

static void tim2_init(void)
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
    tim2_init();

    while (1) {
        if (kestrel_sem_take(&isr_sem, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            fail();
        }
        isr_taken++;
    }
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    uint32_t total_lat_max = 0U;
    uint32_t total_per_min = 0xFFFFFFFFU;
    uint32_t total_per_max = 0U;
    uint32_t prev_q = 0U;
    uint32_t prev_mtx = 0U;
    uint32_t prev_isr = 0U;
    uint32_t prev_taken = 0U;
    stat_t lat;
    stat_t per;

    while (1) {
        kestrel_delay_until(&last, REPORT_PERIOD_TICKS);

        KESTREL_CRITICAL
        {
            lat = lat_win;
            per = per_win;
            stat_reset(&lat_win);
            stat_reset(&per_win);
        }

        if (lat.count == 0U || per.count == 0U) {
            continue;
        }
        if (lat.max > total_lat_max) {
            total_lat_max = lat.max;
        }
        if (per.min < total_per_min) {
            total_per_min = per.min;
        }
        if (per.max > total_per_max) {
            total_per_max = per.max;
        }

        uint32_t qn = q_count;
        uint32_t mn = mtx_count;
        uint32_t in = isr_count;
        uint32_t tn = isr_taken;

        uart_puts(LOAD_ENABLED ? "load" : "idle");
        uart_puts(" t=");
        uart_put_u32(last / 1000U);
        uart_puts("s lat min=");
        uart_put_u32(lat.min);
        uart_puts(" avg=");
        uart_put_u32((uint32_t)(lat.sum / lat.count));
        uart_puts(" max=");
        uart_put_u32(lat.max);
        uart_puts(" per min=");
        uart_put_u32(per.min);
        uart_puts(" max=");
        uart_put_u32(per.max);
        uart_puts(" missed=");
        uart_put_u32(missed);
        uart_puts(" dl=");
        uart_put_u32(deadline_miss);
        uart_puts(" | q/s=");
        uart_put_u32(qn - prev_q);
        uart_puts(" mtx/s=");
        uart_put_u32(mn - prev_mtx);
        uart_puts(" isr/s=");
        uart_put_u32(in - prev_isr);
        uart_puts(" took/s=");
        uart_put_u32(tn - prev_taken);
        uart_puts(" | total lat_max=");
        uart_put_u32(total_lat_max);
        uart_puts(" (");
        uart_put_u32(total_lat_max / CPU_MHZ);
        uart_puts("us) per=");
        uart_put_u32(total_per_min);
        uart_puts("..");
        uart_put_u32(total_per_max);
        uart_puts("\r\n");

        prev_q = qn;
        prev_mtx = mn;
        prev_isr = in;
        prev_taken = tn;
    }
}

static void create_task(kestrel_task_fn_t fn, void *arg, uint8_t priority)
{
    if (kestrel_new_task(fn, arg, priority).status != KESTREL_OK) {
        fail();
    }
}

int main(void)
{
    if (clock_setup_72MHz() != CLOCK_OK) {
        fail();
    }
    systick_init(SystemCoreClock);
    dwt_init();

    output_pin_init(LED_PORT, LED_PIN);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M6.2: jitter under load ===\r\n");

    stat_reset(&lat_win);
    stat_reset(&per_win);

    if (kestrel_queue_init(&q, queue_storage, sizeof(item_t), QUEUE_CAPACITY) != KESTREL_OK) {
        fail();
    }
    if (kestrel_mutex_init(&mtx) != KESTREL_OK) {
        fail();
    }
    if (kestrel_sem_init(&isr_sem, 0U, 1U) != KESTREL_OK) {
        fail();
    }

    create_task(periodic_task, NULL, PRIO_PERIODIC);
    create_task(reporter_task, NULL, PRIO_REPORTER);
#if LOAD_ENABLED
    create_task(producer_task, NULL, PRIO_LOAD);
    create_task(consumer_task, NULL, PRIO_LOAD);
    create_task(mutex_task, NULL, PRIO_LOAD);
    create_task(mutex_task, NULL, PRIO_LOAD);
    create_task(isr_consumer_task, NULL, PRIO_LOAD);
#endif
    kestrel_start();

    while (1) {
    }
}
