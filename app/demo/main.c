#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/semaphore.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define PRIO_BACKGROUND 1U
#define PRIO_PRODUCER   2U
#define PRIO_CONSUMER   3U

#define PRODUCER_ENABLED       false
#define PRODUCER_PERIOD_TICKS  500U
#define CONSUMER_TIMEOUT_TICKS 2000U

static kestrel_sem_t data_ready;

static volatile uint32_t bg_counter[2];
static volatile uint32_t last_give_tick;
static volatile uint32_t give_count;

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

static void consumer_task(void *arg)
{
    (void)arg;
    uint32_t prev = kestrel_get_ticks();
    uint32_t got = 0;
    uint32_t timeouts = 0;

    while (1) {
        kestrel_status_t status = kestrel_sem_take(&data_ready, CONSUMER_TIMEOUT_TICKS);
        uint32_t now = kestrel_get_ticks();

        if (status == KESTREL_OK) {
            got++;
            uart_puts("got     t=");
            uart_put_u32(now);
            uart_puts(" dt=");
            uart_put_u32(now - prev);
            uart_puts(" lat=");
            uart_put_u32(now - last_give_tick);
        } else if (status == KESTREL_ERR_TIMEOUT) {
            timeouts++;
            uart_puts("timeout t=");
            uart_put_u32(now);
            uart_puts(" dt=");
            uart_put_u32(now - prev);
        } else {
            uart_puts("error   t=");
            uart_put_u32(now);
        }

        uart_puts(" got=");
        uart_put_u32(got);
        uart_puts(" timeouts=");
        uart_put_u32(timeouts);
        uart_puts(" gives=");
        uart_put_u32(give_count);
        uart_puts("\r\n");

        prev = now;
    }
}

static void producer_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();
    bool led = false;

    while (1) {
        kestrel_delay_until(&last, PRODUCER_PERIOD_TICKS);

        led = !led;
        gpio_put(LED_PORT, LED_PIN, led);

#if PRODUCER_ENABLED
        last_give_tick = kestrel_get_ticks();
        give_count++;
        (void)kestrel_sem_give(&data_ready);
#endif
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
    uart_puts("\r\n=== Kestrel M4.2: semaphore ===\r\n");

    if (kestrel_sem_init(&data_ready, 0U, 1U) != KESTREL_OK) {
        fail();
    }

    create_task(background_task, (void *)&bg_counter[0], PRIO_BACKGROUND);
    create_task(background_task, (void *)&bg_counter[1], PRIO_BACKGROUND);
    create_task(producer_task, NULL, PRIO_PRODUCER);
    create_task(consumer_task, NULL, PRIO_CONSUMER);
    kestrel_start();

    while (1) {
    }
}
