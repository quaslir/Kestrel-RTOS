#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT   GPIOA
#define LED_PIN    5U
#define PROBE_PORT GPIOA
#define PROBE_PIN  6U

#define PRIO_BACKGROUND 1U
#define PRIO_LED        2U
#define PRIO_CRITICAL   3U

#define CRITICAL_PERIOD_TICKS 1U
#define LED_PERIOD_TICKS      500U
#define REPORT_PERIOD_TICKS   1000U

static volatile uint32_t bg_counter[2];
static volatile uint32_t critical_runs;
static volatile uint32_t critical_late_max;

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

static void critical_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    bool level = false;

    while (1) {
        uint32_t late = kestrel_get_ticks() - last;
        if (late > critical_late_max) {
            critical_late_max = late;
        }
        critical_runs++;

        level = !level;
        gpio_put(PROBE_PORT, PROBE_PIN, level);
        kestrel_delay_until(&last, CRITICAL_PERIOD_TICKS);
    }
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    uint32_t prev_crit = critical_runs;
    uint32_t prev_bg0 = bg_counter[0];
    uint32_t prev_bg1 = bg_counter[1];

    while (1) {
        kestrel_delay_until(&last, REPORT_PERIOD_TICKS);

        uint32_t crit = critical_runs;
        uint32_t bg0 = bg_counter[0];
        uint32_t bg1 = bg_counter[1];

        uart_puts("t=");
        uart_put_u32(last);
        uart_puts(" crit/s=");
        uart_put_u32(crit - prev_crit);
        uart_puts(" late_max=");
        uart_put_u32(critical_late_max);
        uart_puts(" bg0/s=");
        uart_put_u32(bg0 - prev_bg0);
        uart_puts(" bg1/s=");
        uart_put_u32(bg1 - prev_bg1);
        uart_puts("\r\n");

        prev_crit = crit;
        prev_bg0 = bg0;
        prev_bg1 = bg1;
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
        kestrel_delay_until(&last, LED_PERIOD_TICKS);
    }
}

static void background_task(void *arg)
{
    volatile uint32_t *counter = (volatile uint32_t *)arg;

    while (1) {
        (*counter)++;
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

int main(void)
{
    if (clock_setup_72MHz() != CLOCK_OK) {
        fail();
    }
    systick_init(SystemCoreClock);

    output_pin_init(LED_PORT, LED_PIN);
    output_pin_init(PROBE_PORT, PROBE_PIN);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M3.3: priorities ===\r\n");

    kestrel_new_task(background_task, (void *)&bg_counter[0], PRIO_BACKGROUND);
    kestrel_new_task(background_task, (void *)&bg_counter[1], PRIO_BACKGROUND);
    kestrel_new_task(led_task, NULL, PRIO_LED);
    kestrel_new_task(reporter_task, NULL, PRIO_LED);
    kestrel_new_task(critical_task, NULL, PRIO_CRITICAL);
    kestrel_start();

    while (1) {
    }
}
