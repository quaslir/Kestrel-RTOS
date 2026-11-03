#include "gpio.h"
#include "systick.h"
#include <stdint.h>
#include "clock.h"
#include "uart.h"
#include "kestrel/task.h"
#include "kestrel/time.h"
#include "kestrel/critical.h"
void led_task(void *arg)
{
    while (1) {
        // gpio_put(GPIOA, 5, true);
        // gpio_put(GPIOA, 5, false);
        KESTREL_CRITICAL
        {
            uart_puts("AAAAAAAAAAAAAAAAAAAA\r\n");
        }
    }
}

void uart_task(void *arg)
{
    while (1) {
        KESTREL_CRITICAL
        {
            uart_puts("BBBBBBBBBBBBBBBBBBBB\r\n");
        }
    }
}

void print_task(void *arg)
{
    while (1) {
        KESTREL_CRITICAL
        {
            uart_puts("CCCCCCCCCCCCCCCCCCCCCCCC\r\n");
        }
    }
}

int main(void)
{
    clock_setup_72MHz();
    systick_init(72000000);

    gpio_enable_port(GPIOA);
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    gpio_pull(GPIOA, 5, GPIO_PULL_NONE);
    uart_init(115200);
    kestrel_new_task(led_task, NULLPTR);
    kestrel_new_task(uart_task, (void *)0x12);
    kestrel_new_task(print_task, NULLPTR);
    kestrel_start();
    while (1)
        ;
}
