#include "nvmc.h"
#include <string.h>
#include "SEGGER_RTT.h"

// Constants
#define BUFFER_SIZE 128 // Buffer size for UART data (multiple of 4 for word alignment)
#define FLASH_TEST_ADDR 0x00028000 // Valid flash address (page-aligned, within 0x27000-0x3FFFF)
#define RECEIVE_TIMEOUT 1000000 // Timeout for entire 128-byte receive operation

// Fallback pin definitions (from previous RTT output: TXD=02, RXD=03)
#ifndef PIN_TXD
#define PIN_TXD 2
#endif
#ifndef PIN_RXD
#define PIN_RXD 3
#endif

// Helper function to convert uint32_t to hex string
void uint32_to_hex(uint32_t value, char *buf, uint8_t digits) {
    const char hex[] = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) {
        buf[i] = hex[value & 0xF];
        value >>= 4;
    }
    buf[digits] = '\0';
}

// Helper function to send messages via RTT and UART
void send_message(const char *msg) {
    SEGGER_RTT_WriteString(0, msg);
    SEGGER_RTT_WriteString(0, "\n");
    uart_send_string(msg);
    uart_send_string("\n");
}

int main(void) {
    // Initialize RTT
    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "Starting initialization...\n");

    // Initialize UART
    SEGGER_RTT_WriteString(0, "Initializing UART...\n");
    char log[32];
    snprintf(log, sizeof(log), "Using pins: TXD=%u, RXD=%u", PIN_TXD, PIN_RXD);
    SEGGER_RTT_WriteString(0, log);
    SEGGER_RTT_WriteString(0, "\n");
    uart_init();
    send_message("UART initialized.");

    // Initialize NVMC
    SEGGER_RTT_WriteString(0, "Initializing NVMC...\n");
    nvmc_init();
    send_message("NVMC initialized.");

    // Buffer for UART data and flash read-back
    uint32_t rx_buffer[BUFFER_SIZE / FLASH_WORD_SIZE];
    uint32_t read_buffer[BUFFER_SIZE / FLASH_WORD_SIZE];
    char temp_char;

    // Step 1: Receive 128 bytes from UART
    send_message("Send 128 bytes via UART...");
    uint32_t timeout = RECEIVE_TIMEOUT;
    for (uint32_t i = 0; i < BUFFER_SIZE / FLASH_WORD_SIZE; i++) {
        uint32_t word = 0;
        for (int j = 0; j < FLASH_WORD_SIZE; j++) {
            uint32_t byte_idx = i * FLASH_WORD_SIZE + j;
            snprintf(log, sizeof(log), "Waiting for byte %u...", byte_idx);
            SEGGER_RTT_WriteString(0, log);
            SEGGER_RTT_WriteString(0, "\n");
            temp_char = uart_receive_char();
            if (temp_char == 0) {
                send_message("UART receive failed!");
                while (1);
            }
            word |= ((uint32_t)(uint8_t)temp_char << (j * 8));
            snprintf(log, sizeof(log), "Received byte %u: 0x%02X", byte_idx, (uint8_t)temp_char);
            SEGGER_RTT_WriteString(0, log);
            SEGGER_RTT_WriteString(0, "\n");
            if (--timeout == 0) {
                send_message("UART receive timeout!");
                while (1);
            }
        }
        rx_buffer[i] = word;
    }
    send_message("Data received.");

    // Step 2: Erase flash page
    char addr_str[9];
    uint32_to_hex(FLASH_TEST_ADDR, addr_str, 8);
    snprintf(log, sizeof(log), "Erasing flash page at 0x%s...", addr_str);
    send_message(log);
    nvmc_result_t result = nvmc_erase_page(FLASH_TEST_ADDR);
    if (result != NVMC_SUCCESS) {
        send_message("Flash erase failed!");
        while (1);
    }
    send_message("Flash page erased.");

    // Step 3: Write data to flash
    send_message("Writing data to flash...");
    result = nvmc_write_buffer(FLASH_TEST_ADDR, rx_buffer, BUFFER_SIZE);
    if (result != NVMC_SUCCESS) {
        send_message("Flash write failed!");
        while (1);
    }
    send_message("Data written to flash.");

    // Step 4: Read data from flash
    send_message("Reading data from flash...");
    memset(read_buffer, 0, BUFFER_SIZE);
    for (uint32_t i = 0; i < BUFFER_SIZE / FLASH_WORD_SIZE; i++) {
        read_buffer[i] = *((volatile uint32_t *)(FLASH_TEST_ADDR + i * FLASH_WORD_SIZE));
    }

    // Step 5: Output read data
    SEGGER_RTT_WriteString(0, "Read data: ");
    uart_send_string("Read data: ");
    char byte_str[2] = {0, 0};
    for (uint32_t i = 0; i < BUFFER_SIZE / FLASH_WORD_SIZE; i++) {
        for (int j = 0; j < FLASH_WORD_SIZE; j++) {
            char byte = (read_buffer[i] >> (j * 8)) & 0xFF;
            if (byte >= 32 && byte <= 126) {
                byte_str[0] = byte;
                SEGGER_RTT_WriteString(0, byte_str);
                uart_send_string(byte_str);
            } else {
                SEGGER_RTT_WriteString(0, ".");
                uart_send_string(".");
            }
        }
    }
    SEGGER_RTT_WriteString(0, "\n");
    uart_send_string("\n");

    // Step 6: Verify data
    if (memcmp(rx_buffer, read_buffer, BUFFER_SIZE) == 0) {
        send_message("Data verification successful!");
    } else {
        send_message("Data verification failed!");
        while (1);
    }

    // Periodic flash access
    send_message("Periodically reading flash data...");
    while (1) {
        memset(read_buffer, 0, BUFFER_SIZE);
        for (uint32_t i = 0; i < BUFFER_SIZE / FLASH_WORD_SIZE; i++) {
            read_buffer[i] = *((volatile uint32_t *)(FLASH_TEST_ADDR + i * FLASH_WORD_SIZE));
        }
        SEGGER_RTT_WriteString(0, "Flash data: ");
        uart_send_string("Flash data: ");
        for (uint32_t i = 0; i < BUFFER_SIZE / FLASH_WORD_SIZE; i++) {
            for (int j = 0; j < FLASH_WORD_SIZE; j++) {
                char byte = (read_buffer[i] >> (j * 8)) & 0xFF;
                if (byte >= 32 && byte <= 126) {
                    byte_str[0] = byte;
                    SEGGER_RTT_WriteString(0, byte_str);
                    uart_send_string(byte_str);
                } else {
                    SEGGER_RTT_WriteString(0, ".");
                    uart_send_string(".");
                }
            }
        }
        SEGGER_RTT_WriteString(0, "\n");
        uart_send_string("\n");
        for (volatile uint32_t i = 0; i < 1000000; i++);
    }
}