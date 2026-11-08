#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/queue.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

#define MODE_FAST_PRODUCER 1
#define MODE_FAST_CONSUMER 2
#define MODE               MODE_FAST_CONSUMER

#define PRIO_REPORTER 1U
#define PRIO_PRODUCER 2U
#define PRIO_CONSUMER 3U

#define QUEUE_CAPACITY      4U
#define SLOW_PERIOD_TICKS   100U
#define REPORT_PERIOD_TICKS 1000U

typedef struct {
    uint32_t seq;
    uint32_t tick;
} item_t;

static item_t queue_storage[QUEUE_CAPACITY];
static kestrel_queue_t q;

static volatile uint32_t sent;
static volatile uint32_t received;
static volatile uint32_t order_errors;
static volatile uint32_t status_errors;
static volatile uint32_t last_lat;
static volatile uint32_t max_lat;

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

static void producer_task(void *arg)
{
    (void)arg;
    uint32_t seq = 0;
#if MODE == MODE_FAST_CONSUMER
    uint32_t last = kestrel_get_ticks();
#endif

    while (1) {
#if MODE == MODE_FAST_CONSUMER
        kestrel_delay_until(&last, SLOW_PERIOD_TICKS);
#endif
        item_t item = { .seq = seq, .tick = kestrel_get_ticks() };
        if (kestrel_queue_send(&q, &item, KESTREL_WAIT_FOREVER) == KESTREL_OK) {
            seq++;
            sent++;
        } else {
            status_errors++;
        }
    }
}

static void consumer_task(void *arg)
{
    (void)arg;
    uint32_t expected = 0;
#if MODE == MODE_FAST_PRODUCER
    uint32_t last = kestrel_get_ticks();
#endif

    while (1) {
#if MODE == MODE_FAST_PRODUCER
        kestrel_delay_until(&last, SLOW_PERIOD_TICKS);
#endif
        item_t item;
        if (kestrel_queue_receive(&q, &item, KESTREL_WAIT_FOREVER) != KESTREL_OK) {
            status_errors++;
            continue;
        }

        if (item.seq != expected) {
            order_errors++;
        }
        expected = item.seq + 1U;

        last_lat = kestrel_get_ticks() - item.tick;
        if (last_lat > max_lat) {
            max_lat = last_lat;
        }
        received++;
    }
}

static void reporter_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, REPORT_PERIOD_TICKS);

        uart_puts("t=");
        uart_put_u32(last);
        uart_puts(" sent=");
        uart_put_u32(sent);
        uart_puts(" recv=");
        uart_put_u32(received);
        uart_puts(" in_q=");
        uart_put_u32(q.count);
        uart_puts(" order_err=");
        uart_put_u32(order_errors);
        uart_puts(" status_err=");
        uart_put_u32(status_errors);
        uart_puts(" lat=");
        uart_put_u32(last_lat);
        uart_puts(" max_lat=");
        uart_put_u32(max_lat);
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

    output_pin_init(LED_PORT, LED_PIN);

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M4.6: queue ===\r\n");

    if (kestrel_queue_init(&q, queue_storage, sizeof(item_t), QUEUE_CAPACITY) != KESTREL_OK) {
        fail();
    }

    create_task(reporter_task, NULL, PRIO_REPORTER);
    create_task(producer_task, NULL, PRIO_PRODUCER);
    create_task(consumer_task, NULL, PRIO_CONSUMER);
    kestrel_start();

    while (1) {
    }
}
