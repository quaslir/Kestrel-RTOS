#include "uart.h"
#include "gpio.h"

#define UART2_PCLK_DIV 2U

bool uart_init(uint32_t baud)
{
    // GPIO part

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
    if (brr < UART2_MIN_BRR) {
        return false;
    }

    USART2->BRR = brr;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    return true;
}
