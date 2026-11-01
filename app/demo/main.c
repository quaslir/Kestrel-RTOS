#include "gpio.h"
#include "systick.h"
#include <stdint.h>

int main(void)
{
    systick_init(8000000);
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
