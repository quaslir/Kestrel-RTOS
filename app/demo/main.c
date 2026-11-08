#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/mutex.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define PRIO_LOW    1U
#define PRIO_MEDIUM 2U
#define PRIO_HIGH   3U

#define PERIOD_TICKS      1000U
#define HIGH_OFFSET       10U
#define MEDIUM_OFFSET     20U
#define LOW_HOLD_TICKS    50U
#define MEDIUM_SPIN_TICKS 300U

#define HIGH_TIMEOUT_TICKS 5U

static kestrel_mutex_t m;

static volatile uint32_t low_start;
static volatile uint32_t unlock_tick;
static volatile uint32_t high_wait;
static volatile kestrel_status_t high_status;
static volatile uint32_t medium_start;

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

static void low_task(void *arg)
{
    (void)arg;
    uint32_t last = 0;

    while (1) {
        kestrel_delay_until(&last, PERIOD_TICKS);

        if (kestrel_mutex_lock(&m, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            fail();
        }
        low_start = kestrel_get_ticks();
        while ((kestrel_get_ticks() - low_start) < LOW_HOLD_TICKS) {
        }
        unlock_tick = kestrel_get_ticks();
        if (kestrel_mutex_unlock(&m) != KESTREL_OK) {
            fail();
        }

        uart_puts("t=");
        uart_put_u32(low_start);
        uart_puts(high_status == KESTREL_OK ? " high=OK" : " high=TIMEOUT");
        uart_puts(" wait=");
        uart_put_u32(high_wait);
        uart_puts(" med_start=");
        uart_put_u32(medium_start - low_start);
        uart_puts(" med_lat=");
        uart_put_u32(medium_start - unlock_tick);
        uart_puts("\r\n");
    }
}

static void medium_task(void *arg)
{
    (void)arg;
    uint32_t last = MEDIUM_OFFSET;

    while (1) {
        kestrel_delay_until(&last, PERIOD_TICKS);

        medium_start = kestrel_get_ticks();
        while ((kestrel_get_ticks() - medium_start) < MEDIUM_SPIN_TICKS) {
        }
    }
}

static void high_task(void *arg)
{
    (void)arg;
    uint32_t last = HIGH_OFFSET;

    while (1) {
        kestrel_delay_until(&last, PERIOD_TICKS);

        uint32_t before = kestrel_get_ticks();
        kestrel_status_t status = kestrel_mutex_lock(&m, HIGH_TIMEOUT_TICKS);
        high_wait = kestrel_get_ticks() - before;
        high_status = status;

        if (status == KESTREL_OK) {
            if (kestrel_mutex_unlock(&m) != KESTREL_OK) {
                fail();
            }
        }
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

    output_pin_init(LED_PORT, LED_PIN);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M4.4: priority inheritance ===\r\n");

    if (kestrel_mutex_init(&m) != KESTREL_OK) {
        fail();
    }

    create_task(low_task, NULL, PRIO_LOW);
    create_task(medium_task, NULL, PRIO_MEDIUM);
    create_task(high_task, NULL, PRIO_HIGH);
    kestrel_start();

    while (1) {
    }
}
