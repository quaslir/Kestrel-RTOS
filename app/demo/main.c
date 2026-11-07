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

#define PRIO_PRINTER 1U
#define PRIO_CHECKER 2U

static kestrel_mutex_t uart_mutex;

static void check(const char *name, bool ok)
{
    uart_puts(ok ? "PASS " : "FAIL ");
    uart_puts(name);
    uart_puts("\r\n");
}

static void checker_task(void *arg)
{
    (void)arg;

    check("lock free mutex", kestrel_mutex_lock(&uart_mutex, KESTREL_NO_WAIT) == KESTREL_OK);
    check("lock twice is error", kestrel_mutex_lock(&uart_mutex, KESTREL_NO_WAIT) != KESTREL_OK);
    check("unlock own mutex", kestrel_mutex_unlock(&uart_mutex) == KESTREL_OK);
    check("unlock twice is error", kestrel_mutex_unlock(&uart_mutex) != KESTREL_OK);
    uart_puts("--- printers start ---\r\n");
}

static void printer_task(void *arg)
{
    const char *line = (const char *)arg;

    while (1) {
        kestrel_mutex_lock(&uart_mutex, KESTREL_WAIT_FOREVER);
        uart_puts(line);
        kestrel_mutex_unlock(&uart_mutex);
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

    if (!uart_init(115200U)) {
        fail();
    }
    uart_puts("\r\n=== Kestrel M4.3: mutex ===\r\n");

    if (kestrel_mutex_init(&uart_mutex) != KESTREL_OK) {
        fail();
    }

    create_task(checker_task, NULL, PRIO_CHECKER);
    create_task(printer_task, (void *)"AAAAAAAAAAAAAAAAAAAA\r\n", PRIO_PRINTER);
    create_task(printer_task, (void *)"BBBBBBBBBBBBBBBBBBBB\r\n", PRIO_PRINTER);
    kestrel_start();

    while (1) {
    }
}
