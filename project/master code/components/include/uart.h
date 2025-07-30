#ifndef UART_H
#define UART_H

#include <stdint.h>

// UART Pins
#define PIN_TXD  2   // TX Pin
#define PIN_RXD  3   // RX Pin

// UART Functions
uint32_t uart_init(void);
void uart_send_string(const char *str);
void uart_send_byte(char byte);
char uart_receive_char(void);

#endif // UART_H
