#include "stm32f3xx.h"
#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>
void gpio_enable_port(GPIO_TypeDef *port)
{
    switch ((uintptr_t)port) {
    case GPIOA_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
        break;
    case GPIOB_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
        break;
    case GPIOC_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
        break;
    case GPIOD_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIODEN;
        break;
    case GPIOE_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOEEN;
        break;
    case GPIOF_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOFEN;
        break;
    case GPIOG_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOGEN;
        break;
    case GPIOH_BASE:
        RCC->AHBENR |= RCC_AHBENR_GPIOHEN;
        break;
    default:
        return;
    }
    (void)RCC->AHBENR;
}

void gpio_set_mode(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode)
{
    GPIO_RETURN_IF_BAD_PIN(pin);
    uint8_t shift = GPIO_SHIFT_2BIT(pin); // start pin * 2
    port->MODER &= ~(0x3U << shift);      // clear bits
    port->MODER |= ((uint32_t)mode << shift);
}

void gpio_pull(GPIO_TypeDef *port, uint8_t pin, gpio_pull_t mode)
{
    GPIO_RETURN_IF_BAD_PIN(pin);
    uint8_t shift = GPIO_SHIFT_2BIT(pin);
    port->PUPDR &= ~(0x3U << shift);
    port->PUPDR |= ((uint32_t)mode << shift);
}

void gpio_set_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af)
{
    GPIO_RETURN_IF_BAD_PIN(pin);
    if (af > 15) {
        return;
    }
    uint8_t reg = pin / 8;
    uint8_t pos = pin % 8;
    uint8_t shift = pos * 4;
    uint32_t mask = (0xFU << shift);
    uint32_t value = ((uint32_t)af) << shift;
    port->AFR[reg] &= ~mask;
    port->AFR[reg] |= value;
}
void gpio_put(GPIO_TypeDef *port, uint8_t pin, bool on)
{
    GPIO_RETURN_IF_BAD_PIN(pin);
    uint32_t mask = (on ? 1U << pin : 1U << (pin + 16));

    port->BSRR = mask;
}

void gpio_set_otype(GPIO_TypeDef *port, uint8_t pin, gpio_otype_t otype)
{
    GPIO_RETURN_IF_BAD_PIN(pin);
    uint32_t mask = 0x1U << pin;
    port->OTYPER &= ~mask;
    port->OTYPER |= ((uint32_t)otype << pin);
}
void gpio_set_speed(GPIO_TypeDef *port, uint8_t pin, gpio_speed_t speed)
{
    GPIO_RETURN_IF_BAD_PIN(pin);
    uint8_t shift = GPIO_SHIFT_2BIT(pin);
    uint32_t mask = 0x3U << shift;
    port->OSPEEDR &= ~mask;
    port->OSPEEDR |= ((uint32_t)speed << shift);
}
