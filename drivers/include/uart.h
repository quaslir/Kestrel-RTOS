#ifndef KESTREL_UART_H
#define KESTREL_UART_H
#include <stdint.h>
#include <stdbool.h>
#define UART2_PORT    GPIOA
#define UART2_TX_PIN  2U
#define UART2_RX_PIN  3U
#define UART2_AF      7U
#define UART2_MIN_BRR 16U
bool uart_init(uint32_t baud); // returns actual freq
void uart_putc(char c);
void uart_puts(const char *str);
#endif
