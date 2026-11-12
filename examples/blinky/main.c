/*
 * Two independent tasks: one blinks the LED on PA5 at 1 Hz, the other prints
 * the uptime over UART (115200 baud) once per second.
 */
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

static void blink_task(void *arg)
{
    (void)arg;
    bool on = false;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, 500U);
        on = !on;
        gpio_put(LED_PORT, LED_PIN, on);
    }
}

static void uptime_task(void *arg)
{
    (void)arg;
    uint32_t last = kestrel_get_ticks();

    while (1) {
        kestrel_delay_until(&last, 1000U);
        uart_puts("uptime ");
        put_u32(last / 1000U);
        uart_puts(" s\r\n");
    }
}

int main(void)
{
    clock_setup_72MHz();
    systick_init(SystemCoreClock);
    uart_init(115200U);

    gpio_enable_port(LED_PORT);
    gpio_set_mode(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT);

    kestrel_new_task(blink_task, NULL, 2U);
    kestrel_new_task(uptime_task, NULL, 1U);
    kestrel_start();
}
