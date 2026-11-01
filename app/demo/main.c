#include "gpio.h"
#include "systick.h"
#include <stdint.h>
#include "clock.h"
int main(void)
{
    clock_setup_72MHz();
    systick_init(72000000);

    gpio_enable_port();
    gpio_set_dir();
    gpio_set_output_mode();

    while (1) {
        gpio_put(true);
        systick_delay_ms(200);
        gpio_put(false);
        systick_delay_ms(200);
    }
}
