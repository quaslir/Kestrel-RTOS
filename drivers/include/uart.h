#ifndef KESTREL_UART_H
#define KESTREL_UART_H
#include <stdint.h>
#include <stdbool.h>
#define UART2_PORT    GPIOA
#define UART2_TX_PIN  2U
#define UART2_RX_PIN  3U
#define UART2_AF      7U
#define UART2_MIN_BRR 16U
#define UART2_MAX_BRR 0xFFFFU

/* USART2 on PA2/PA3 (ST-LINK virtual COM port), 8N1. Returns false if baud is 0
 * or the resulting BRR is outside UART2_MIN_BRR..UART2_MAX_BRR. */
bool uart_init(uint32_t baud);
void uart_putc(char c);
void uart_puts(const char *str);
#endif
