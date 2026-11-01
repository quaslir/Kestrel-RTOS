#ifndef GPIO_H
#define GPIO_H
#include <stdbool.h>
void gpio_enable_port(void);     // enables GPIOA port
void gpio_set_output_mode(void); // sets no pull mode
void gpio_set_dir(void);         // sets output direction
void gpio_put(bool on);
#endif
