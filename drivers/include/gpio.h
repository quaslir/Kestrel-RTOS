#ifndef KESTREL_GPIO_H
#define KESTREL_GPIO_H
#include <stdbool.h>
#include "stm32f3xx.h"
#include <stdint.h>

#define GPIO_PIN_COUNT 16U

#define GPIO_RETURN_IF_BAD_PIN(pin)        \
    do {                                   \
        if ((pin) >= GPIO_PIN_COUNT) {     \
            return;                        \
        }                                  \
    } while (0)

#define GPIO_SHIFT_2BIT(pin) ((uint32_t)(pin) * 2U)

typedef enum {
    GPIO_MODE_INPUT  = 0x0, /* 00 */
    GPIO_MODE_OUTPUT = 0x1, /* 01 */
    GPIO_MODE_AF     = 0x2, /* 10 */
    GPIO_MODE_ANALOG = 0x3, /* 11 */
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE = 0x0, /* 00 */
    GPIO_PULL_UP   = 0x1, /* 01 */
    GPIO_PULL_DOWN = 0x2, /* 10 */
} gpio_pull_t;

typedef enum {
    GPIO_OTYPE_PUSH_PULL  = 0x0,
    GPIO_OTYPE_OPEN_DRAIN = 0x1,
} gpio_otype_t;

typedef enum {
    GPIO_SPEED_LOW    = 0x0, /* x0 */
    GPIO_SPEED_MEDIUM = 0x1, /* 01 */
    GPIO_SPEED_HIGH   = 0x3, /* 11 */
} gpio_speed_t;




void gpio_enable_port(GPIO_TypeDef * port);
void gpio_set_mode( GPIO_TypeDef * port, uint8_t pin, gpio_mode_t mode);
void gpio_pull(GPIO_TypeDef * port, uint8_t pin, gpio_pull_t mode);
void gpio_set_af( GPIO_TypeDef * port, uint8_t pin, uint8_t af);
void gpio_put(GPIO_TypeDef * port, uint8_t pin ,bool on);
void gpio_set_otype(GPIO_TypeDef * port, uint8_t pin, gpio_otype_t otype);
void gpio_set_speed(GPIO_TypeDef* port, uint8_t pin, gpio_speed_t speed);
#endif
