#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define USE_FPU 1

#define PRIO_PINGPONG       2U
#define PRIO_REPORTER       3U
#define REPORT_PERIOD_TICKS 1000U

typedef struct {
    uint32_t count;
    uint32_t disturbed;
    uint32_t min;
    uint32_t max;
    uint64_t sum;
} stats_t;

static volatile uint32_t switch_start;
static volatile uint32_t switch_tick;
static volatile stats_t window = { 0, 0, 0xFFFFFFFFU, 0, 0 };
static uint32_t total_min = 0xFFFFFFFFU;
static uint32_t total_max;

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

static void record(uint32_t cycles)
{
    window.count++;
    window.sum += cycles;
    if (cycles < window.min) {
        window.min = cycles;
    }
    if (cycles > window.max) {
        window.max = cycles;
    }
}

static void touch_fpu(void)
{
#if USE_FPU
    __asm volatile("vmov s0, %0\n" : : "r"(1U) : "s0");
#endif
}

static void mark_and_yield(void)
{
    switch_tick = kestrel_get_ticks();
    switch_start = DWT->CYCCNT;
    kestrel_yield();
}

static void pingpong_task(void *arg)
{
    (void)arg;
    touch_fpu();
    mark_and_yield();

    while (1) {
        uint32_t tick_before = kestrel_get_ticks();
        uint32_t now = DWT->CYCCNT;
        uint32_t start = switch_start;
        uint32_t start_tick = switch_tick;
        uint32_t now_tick = kestrel_get_ticks();

        if (tick_before == start_tick && now_tick == start_tick) {
            record(now - start);
        } else {
            window.disturbed++;
        }

        mark_and_yield();
    }
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, REPORT_PERIOD_TICKS);

        stats_t s = window;
        window.count = 0;
        window.disturbed = 0;
        window.min = 0xFFFFFFFFU;
        window.max = 0;
        window.sum = 0;

        if (s.count == 0U) {
            continue;
        }
        if (s.min < total_min) {
            total_min = s.min;
        }
        if (s.max > total_max) {
            total_max = s.max;
        }

        uart_puts(USE_FPU ? "fpu" : "int");
        uart_puts(" t=");
        uart_put_u32(last / 1000U);
        uart_puts("s n=");
        uart_put_u32(s.count);
        uart_puts(" dist=");
        uart_put_u32(s.disturbed);
        uart_puts(" min=");
        uart_put_u32(s.min);
        uart_puts(" avg=");
        uart_put_u32((uint32_t)(s.sum / s.count));
        uart_puts(" max=");
        uart_put_u32(s.max);
        uart_puts(" | total min=");
        uart_put_u32(total_min);
        uart_puts(" max=");
        uart_put_u32(total_max);
        uart_puts("\r\n");
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
    uart_puts("\r\n=== Kestrel M6.1: context switch ===\r\n");

    create_task(pingpong_task, NULL, PRIO_PINGPONG);
    create_task(pingpong_task, NULL, PRIO_PINGPONG);
    create_task(reporter_task, NULL, PRIO_REPORTER);
    kestrel_start();

    while (1) {
    }
}
