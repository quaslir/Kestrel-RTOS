#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT     GPIOA
#define LED_PIN      5U
#define PERIOD_TICKS 1000U
#define WORK_TICKS   3U
#define UNTIL_OFFSET 500

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

static void report(const char *name, uint32_t now, uint32_t prev)
{
    uart_puts(name);
    uart_puts(" t=");
    uart_put_u32(now);
    uart_puts(" dt=");
    uart_put_u32(now - prev);
    uart_puts("\r\n");
}

static void fake_work(void)
{
    uint32_t start = kestrel_get_ticks();
    while (kestrel_get_ticks() - start < WORK_TICKS) {
    }
}

static void led_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    bool on = false;

    while (1) {
        on = !on;
        gpio_put(LED_PORT, LED_PIN, on);
        kestrel_delay_until(&last, PERIOD_TICKS / 2U);
    }
}

static void drift_task(void *arg)
{
    (void)arg;
    uint32_t prev = kestrel_get_ticks();

    while (1) {
        uint32_t now = kestrel_get_ticks();
        report("delay      ", now, prev);
        prev = now;

        fake_work();
        kestrel_delay(PERIOD_TICKS);
    }
}

static void until_task(void *arg)
{
    (void)arg;
    kestrel_delay(UNTIL_OFFSET);

    uint32_t last = kestrel_get_ticks();
    uint32_t prev = last;

    while (1) {
        uint32_t now = kestrel_get_ticks();
        report("delay_until", now, prev);
        prev = now;

        fake_work();
        kestrel_delay_until(&last, PERIOD_TICKS);
    }
}

static void fail(void)
{
    gpio_enable_port(LED_PORT);
    gpio_set_mode(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT);
    gpio_put(LED_PORT, LED_PIN, true);
    while (1) {
    }
}

int main(void)
{
    if (clock_setup_72MHz() != CLOCK_OK) {
        fail();
    }
    systick_init(SystemCoreClock);

    gpio_enable_port(LED_PORT);
    gpio_set_mode(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pull(LED_PORT, LED_PIN, GPIO_PULL_NONE);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M3.2: delay vs delay_until ===\r\n");

    kestrel_new_task(led_task, NULL);
    kestrel_new_task(drift_task, NULL);
    kestrel_new_task(until_task, NULL);
    kestrel_start();

    while (1) {
    }
}
