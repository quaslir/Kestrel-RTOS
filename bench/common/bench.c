#include "bench.h"

#include <stdbool.h>

#include "clock.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"

#define LED_PORT GPIOA
#define LED_PIN  5U

static void led_init(void)
{
    gpio_enable_port(LED_PORT);
    gpio_set_mode(LED_PORT, LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_pull(LED_PORT, LED_PIN, GPIO_PULL_NONE);
    gpio_put(LED_PORT, LED_PIN, false);
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void bench_fail(void)
{
    led_init();
    gpio_put(LED_PORT, LED_PIN, true);
    while (1) {
    }
}

void bench_board_init(const char *name)
{
    if (clock_setup_72MHz() != CLOCK_OK) {
        bench_fail();
    }
    systick_init(SystemCoreClock);
    dwt_init();
    led_init();

    if (!uart_init(115200U)) {
        bench_fail();
    }
    uart_puts("\r\n=== Kestrel ");
    uart_puts(name);
    uart_puts(" ===\r\n");
}

void bench_create_task(kestrel_task_fn_t fn, void *arg, uint8_t priority)
{
    if (kestrel_new_task(fn, arg, priority).status != KESTREL_OK) {
        bench_fail();
    }
}

void uart_put_u32(uint32_t value)
{
    char buf[10];
    uint8_t len = 0U;

    do {
        buf[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);

    while (len > 0U) {
        uart_putc(buf[--len]);
    }
}
