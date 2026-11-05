#include "uart.h"
#include "gpio.h"

#define UART2_PCLK_DIV 2U

bool uart_init(uint32_t baud)
{
    if (baud == 0) {
        return false;
        // GPIO part
    }
    gpio_enable_port(UART2_PORT);

    gpio_set_af(UART2_PORT, UART2_TX_PIN, UART2_AF);
    gpio_set_mode(UART2_PORT, UART2_TX_PIN, GPIO_MODE_AF);
    gpio_pull(UART2_PORT, UART2_TX_PIN, GPIO_PULL_NONE);

    gpio_set_af(UART2_PORT, UART2_RX_PIN, UART2_AF);
    gpio_set_mode(UART2_PORT, UART2_RX_PIN, GPIO_MODE_AF);
    gpio_pull(UART2_PORT, UART2_RX_PIN, GPIO_PULL_NONE);

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;
    uint32_t apb1_freq = SystemCoreClock / UART2_PCLK_DIV;
    uint32_t brr = (apb1_freq + (baud / 2)) / baud;
    if (brr < UART2_MIN_BRR || brr > 0xFFFF) {
        return false;
    }

    USART2->BRR = brr;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    return true;
}

void uart_putc(char c)
{
    while (!(USART2->ISR & USART_ISR_TXE))
        ;
    USART2->TDR = (uint8_t)c;
}

void uart_puts(const char *str)
{
    for (uint32_t i = 0; str[i]; i++) {
        uart_putc(str[i]);
    }
}
