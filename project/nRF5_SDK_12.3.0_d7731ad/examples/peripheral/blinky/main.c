#include <stdint.h>
#include "nrf.h"
#include "nrf_delay.h"
#include "SEGGER_RTT.h"

// LCD and I2C definitions
#define TWI_SCL_PIN      4    // P0.04
#define TWI_SDA_PIN      3    // P0.03
#define LCD_I2C_ADDRESS  0x27 // PCF8574 address (try 0x3F if this fails)
#define LCD_BACKLIGHT    0x08 // Backlight control
#define LCD_EN           0x04 // Enable bit
#define LCD_RW           0x02 // Read/Write bit
#define LCD_RS           0x01 // Register Select bit
#define LCD_WIDTH        16   // LCD columns
#define LCD_LINES        2    // LCD rows

static uint8_t m_backlight = LCD_BACKLIGHT;

// Convert uint8_t to hex string for RTT logging
static void uint8_to_hex(uint8_t value, char *buf) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(value >> 4) & 0x0F];
    buf[1] = hex[value & 0x0F];
    buf[2] = '\0';
}

// Low-level TWI write function with RTT debugging
static uint32_t twi_write(uint8_t address, uint8_t *data, uint8_t length) {
    char addr_str[3];
    char data_str[3];
    uint8_to_hex(address, addr_str);
    SEGGER_RTT_WriteString(0, "TWI Write: Address=0x");
    SEGGER_RTT_WriteString(0, addr_str);
    SEGGER_RTT_WriteString(0, ", Length=");
    uint8_to_hex(length, data_str);
    SEGGER_RTT_WriteString(0, data_str);
    SEGGER_RTT_WriteString(0, "\n");

    for (uint8_t i = 0; i < length; i++) {
        uint8_to_hex(data[i], data_str);
        SEGGER_RTT_WriteString(0, "  Data[");
        uint8_to_hex(i, addr_str);
        SEGGER_RTT_WriteString(0, addr_str);
        SEGGER_RTT_WriteString(0, "]=0x");
        SEGGER_RTT_WriteString(0, data_str);
        SEGGER_RTT_WriteString(0, "\n");
    }

    NRF_TWI0->ADDRESS = (address >> 1); // 7-bit address
    NRF_TWI0->EVENTS_TXDSENT = 0;
    NRF_TWI0->EVENTS_ERROR = 0;
    NRF_TWI0->TASKS_STARTTX = 1;

    for (uint8_t i = 0; i < length; i++) {
        NRF_TWI0->TXD = data[i];
        uint32_t timeout = 100000;
        while (NRF_TWI0->EVENTS_TXDSENT == 0 && NRF_TWI0->EVENTS_ERROR == 0 && timeout--) {
            __WFE();
        }

        if (NRF_TWI0->EVENTS_TXDSENT) {
            uint8_to_hex(i, data_str);
            SEGGER_RTT_WriteString(0, "TWI TXDSENT for byte ");
            SEGGER_RTT_WriteString(0, data_str);
            SEGGER_RTT_WriteString(0, "\n");
            NRF_TWI0->EVENTS_TXDSENT = 0;
        } else if (NRF_TWI0->EVENTS_ERROR) {
            uint32_t error = NRF_TWI0->ERRORSRC;
            SEGGER_RTT_WriteString(0, "TWI ERROR: ");
            if (error & TWI_ERRORSRC_ANACK_Msk) {
                SEGGER_RTT_WriteString(0, "Address NACK\n");
            }
            if (error & TWI_ERRORSRC_DNACK_Msk) {
                SEGGER_RTT_WriteString(0, "Data NACK\n");
            }
            NRF_TWI0->EVENTS_ERROR = 0;
            NRF_TWI0->TASKS_STOP = 1;
            while (NRF_TWI0->EVENTS_STOPPED == 0);
            NRF_TWI0->EVENTS_STOPPED = 0;
            return NRF_ERROR_INVALID_STATE;
        } else {
            SEGGER_RTT_WriteString(0, "TWI Timeout\n");
            NRF_TWI0->TASKS_STOP = 1;
            while (NRF_TWI0->EVENTS_STOPPED == 0);
            NRF_TWI0->EVENTS_STOPPED = 0;
            return NRF_ERROR_TIMEOUT;
        }
    }

    NRF_TWI0->TASKS_STOP = 1;
    while (NRF_TWI0->EVENTS_STOPPED == 0);
    NRF_TWI0->EVENTS_STOPPED = 0;
    SEGGER_RTT_WriteString(0, "TWI Write Complete\n");
    return NRF_SUCCESS;
}

// I2C address scanner
static void i2c_scan(void) {
    SEGGER_RTT_WriteString(0, "Scanning I2C bus...\n");
    uint8_t dummy = 0;
    for (uint8_t addr = 0x08; addr <= 0x7F; addr++) {
        uint32_t result = twi_write(addr, &dummy, 1);
        if (result == NRF_SUCCESS) {
            char addr_str[3];
            uint8_to_hex(addr, addr_str);
            SEGGER_RTT_WriteString(0, "Device found at address 0x");
            SEGGER_RTT_WriteString(0, addr_str);
            SEGGER_RTT_WriteString(0, "\n");
        }
    }
    SEGGER_RTT_WriteString(0, "I2C scan complete\n");
}

// Initialize TWI peripheral
static void twi_init(void) {
    SEGGER_RTT_WriteString(0, "Initializing TWI...\n");

    // Configure pins (open-drain with pull-ups)
    NRF_GPIO->PIN_CNF[TWI_SCL_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                     (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                     (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                                     (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);
    NRF_GPIO->PIN_CNF[TWI_SDA_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
                                     (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
                                     (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                                     (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos);

    // Configure TWI
    NRF_TWI0->PSELSCL = TWI_SCL_PIN;
    NRF_TWI0->PSELSDA = TWI_SDA_PIN;
    NRF_TWI0->FREQUENCY = TWI_FREQUENCY_FREQUENCY_K100 << TWI_FREQUENCY_FREQUENCY_Pos; // 100 kHz
    NRF_TWI0->ENABLE = TWI_ENABLE_ENABLE_Enabled << TWI_ENABLE_ENABLE_Pos;

    // Clear events
    NRF_TWI0->EVENTS_TXDSENT = 0;
    NRF_TWI0->EVENTS_RXDREADY = 0;
    NRF_TWI0->EVENTS_ERROR = 0;

    SEGGER_RTT_WriteString(0, "TWI Initialized\n");
}

// Write 4-bit nibble to LCD
static void lcd_write_nibble(uint8_t nibble, uint8_t rs) {
    char nibble_str[3];
    uint8_to_hex(nibble, nibble_str);
    SEGGER_RTT_WriteString(0, "LCD Nibble: 0x");
    SEGGER_RTT_WriteString(0, nibble_str);
    SEGGER_RTT_WriteString(0, ", RS=");
    SEGGER_RTT_WriteString(0, rs ? "1\n" : "0\n");

    uint8_t data = (nibble << 4) | m_backlight | (rs ? LCD_RS : 0);
    uint8_t buffer[2] = {data | LCD_EN, data}; // Enable pulse
    twi_write(LCD_I2C_ADDRESS, buffer, 2);
    nrf_delay_us(100); // Increased delay
}

// Write 8-bit byte to LCD
static void lcd_write_byte(uint8_t byte, uint8_t rs) {
    char byte_str[3];
    uint8_to_hex(byte, byte_str);
    SEGGER_RTT_WriteString(0, "LCD Byte: 0x");
    SEGGER_RTT_WriteString(0, byte_str);
    SEGGER_RTT_WriteString(0, ", RS=");
    SEGGER_RTT_WriteString(0, rs ? "1\n" : "0\n");

    lcd_write_nibble(byte >> 4, rs);   // High nibble
    lcd_write_nibble(byte & 0x0F, rs); // Low nibble
}

// Initialize LCD with extended delays
static void lcd_init(void) {
    SEGGER_RTT_WriteString(0, "Initializing LCD...\n");
    nrf_delay_ms(100); // Extended power-up delay
    lcd_write_nibble(0x03, 0); // Wake up
    nrf_delay_ms(10);
    lcd_write_nibble(0x03, 0);
    nrf_delay_ms(10);
    lcd_write_nibble(0x03, 0);
    nrf_delay_ms(10);
    lcd_write_nibble(0x02, 0); // Set 4-bit mode
    nrf_delay_ms(10);
    lcd_write_byte(0x28, 0);   // 4-bit, 2 lines, 5x8 font
    nrf_delay_ms(10);
    lcd_write_byte(0x08, 0);   // Display off
    nrf_delay_ms(10);
    lcd_write_byte(0x01, 0);   // Clear display
    nrf_delay_ms(10);
    lcd_write_byte(0x06, 0);   // Entry mode: increment, no shift
    nrf_delay_ms(10);
    lcd_write_byte(0x0C, 0);   // Display on, cursor off, no blink
    nrf_delay_ms(10);
    SEGGER_RTT_WriteString(0, "LCD Initialized\n");
}

// Clear LCD
static void lcd_clear(void) {
    SEGGER_RTT_WriteString(0, "Clearing LCD\n");
    lcd_write_byte(0x01, 0);
    nrf_delay_ms(10);
}

// Set cursor position
static void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0) ? 0x00 : 0x40;
    address += col;
    char addr_str[3];
    uint8_to_hex(address, addr_str);
    SEGGER_RTT_WriteString(0, "Set Cursor: Row=");
    SEGGER_RTT_WriteString(0, row ? "1" : "0");
    SEGGER_RTT_WriteString(0, ", Col=");
    uint8_to_hex(col, addr_str);
    SEGGER_RTT_WriteString(0, addr_str);
    SEGGER_RTT_WriteString(0, ", Addr=0x");
    uint8_to_hex(address, addr_str);
    SEGGER_RTT_WriteString(0, addr_str);
    SEGGER_RTT_WriteString(0, "\n");
    lcd_write_byte(0x80 | address, 0);
}

// Write string to LCD
static void lcd_write_string(const char *str) {
    SEGGER_RTT_WriteString(0, "Writing String: ");
    SEGGER_RTT_WriteString(0, str);
    SEGGER_RTT_WriteString(0, "\n");
    for (uint8_t i = 0; str[i] != '\0' && i < LCD_WIDTH; i++) {
        lcd_write_byte(str[i], 1);
    }
}
int main(void) {
    ret_code_t err_code;

    const app_timer_config_t timer_cfg = APP_TIMER_CONFIG_INITIALIZER;
    err_code = app_timer_init(&timer_cfg);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    i2c_init();
    i2c_scan(); // Only this

    while (1) {
        NRF_LOG_FLUSH();
    }
}
