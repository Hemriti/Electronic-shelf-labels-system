#include "uart.h"

#include <nrf.h>

void uart_init(void) {
    // Configure TX as output, RX as input
    NRF_GPIO->DIRSET = (1 << PIN_TXD);
    NRF_GPIO->DIRCLR = (1 << PIN_RXD);

    // Configure UART registers
    NRF_UART0->PSELTXD   = PIN_TXD;
    NRF_UART0->PSELRXD   = PIN_RXD;
    NRF_UART0->BAUDRATE  = UART_BAUDRATE_BAUDRATE_Baud115200;
    NRF_UART0->CONFIG    = (UART_CONFIG_HWFC_Disabled << UART_CONFIG_HWFC_Pos);

    // Enable UART and start reception
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled;
    NRF_UART0->TASKS_STARTRX = 1;
}

void uart_send_byte(char byte) {
    NRF_UART0->TASKS_STARTTX = 1;
    NRF_UART0->TXD = byte;
    while (!NRF_UART0->EVENTS_TXDRDY);
    NRF_UART0->EVENTS_TXDRDY = 0;
    NRF_UART0->TASKS_STOPTX = 1;
}

void uart_send_string(const char *str) {
    while (*str) {
        uart_send_byte(*str++);
    }
}

char uart_receive_char(void) {
    while (!NRF_UART0->EVENTS_RXDRDY);
    NRF_UART0->EVENTS_RXDRDY = 0;
    return NRF_UART0->RXD;
}
