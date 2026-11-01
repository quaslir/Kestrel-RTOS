#include "gpio.h"
#include <stdint.h>
static void sleep_ms(uint32_t ms) {
    uint32_t itr = ms * (8000000 / 1000) / 10;
    for(volatile uint32_t i = 0; i < itr; i++) {}
}


int main(void) {
    gpio_enable_port();
    gpio_set_dir();
    gpio_set_output_mode();


    while(1) {
        gpio_put(true);
        sleep_ms(1000);
        gpio_put(false);

        sleep_ms(1000);
    }
}
