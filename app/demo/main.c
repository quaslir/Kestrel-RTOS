#include "gpio.h"
#include "systick.h"
#include <stdint.h>
#include "clock.h"
#include "uart.h"
int main(void)
{
    clock_setup_72MHz();
    systick_init(72000000);

    gpio_enable_port(GPIOA);
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    gpio_pull(GPIOA, 5, GPIO_PULL_NONE);
    if (!uart_init(115200)) {
        while (1)
            ;
    }

    while (1) {
        uart_puts("Test\0");
        gpio_put(GPIOA, 5, true);
        systick_delay_ms(1000);
        gpio_put(GPIOA, 5, false);
        systick_delay_ms(1000);
    }
}
