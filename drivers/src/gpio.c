#include "stm32f3xx.h"
#include "gpio.h"
#include <stdbool.h>
void gpio_enable_port(void) {
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN;
   (void) RCC->AHBENR;
}


void gpio_set_dir(void) {
    GPIOA->MODER &= ~GPIO_MODER_MODER5; // clear bits
    GPIOA->MODER |= GPIO_MODER_MODER5_0; // enable output direction
}

void gpio_set_output_mode(void) {
GPIOA->OTYPER &= ~GPIO_OTYPER_OT_5;
GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR5;
GPIOA->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR5;
}

void gpio_put(bool on) {
    GPIOA->BSRR = on ? GPIO_BSRR_BS_5 : GPIO_BSRR_BR_5;
}
