/*
 * A producer sends a small struct every 100 ms; a consumer blocks on the
 * queue and prints every item it receives over UART (115200 baud).
 */
#include <stddef.h>
#include <stdint.h>

#include "stm32f3xx.h"

#include "clock.h"
#include "systick.h"
#include "uart.h"

#include "kestrel/queue.h"
#include "kestrel/task.h"
#include "kestrel/time.h"

typedef struct {
    uint32_t seq;
    uint32_t tick;
} sample_t;

static sample_t storage[4];
static kestrel_queue_t queue;

static void put_u32(uint32_t value)
{
    char buf[10];
    uint32_t len = 0U;
    do {
        buf[len++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    while (len > 0U) {
        uart_putc(buf[--len]);
    }
}

static void producer_task(void *arg)
{
    (void)arg;
    sample_t s = { 0U, 0U };
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, 100U);
        s.seq++;
        s.tick = kestrel_get_ticks();
        kestrel_queue_send(&queue, &s, KESTREL_WAIT_FOREVER);
    }
}

static void consumer_task(void *arg)
{
    (void)arg;
    sample_t s;

    while (1) {
        kestrel_queue_receive(&queue, &s, KESTREL_WAIT_FOREVER);
        uart_puts("sample ");
        put_u32(s.seq);
        uart_puts(" sent at tick ");
        put_u32(s.tick);
        uart_puts("\r\n");
    }
}

int main(void)
{
    clock_setup_72MHz();
    systick_init(SystemCoreClock);
    uart_init(115200U);
    kestrel_queue_init(&queue, storage, sizeof(sample_t), 4U);

    kestrel_new_task(producer_task, NULL, 2U);
    kestrel_new_task(consumer_task, NULL, 1U);
    kestrel_start();
}
