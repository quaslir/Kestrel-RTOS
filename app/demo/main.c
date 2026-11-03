#include "gpio.h"
#include "systick.h"
#include <stdint.h>
#include "clock.h"
#include "uart.h"
#include "kestrel/task.h"
#include "kestrel/time.h"
void led_task(void *arg)
{
    while (1) {
        gpio_put(GPIOA, 5, true);
        kestrel_delay_ms(1000);
        gpio_put(GPIOA, 5, false);
        kestrel_delay_ms(1000);
    }
}

void uart_task(void *arg)
{
    while (1) {
        uart_puts("Test\0");
        kestrel_delay_ms(1000);
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
    kestrel_start();
    while (1)
        ;
}
