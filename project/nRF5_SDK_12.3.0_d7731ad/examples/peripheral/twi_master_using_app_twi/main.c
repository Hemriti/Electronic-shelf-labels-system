#include "nrf_delay.h"
#include <stdbool.h>
#include <stdint.h>

// Pin Definitions
#define SDA_PIN 3
#define SCL_PIN 4

#define LCD_ADDR 0x27 // Change to 0x3F if needed

#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04

// I2C communication delay
void i2c_delay() {
    nrf_delay_us(5); // Approx 100 kHz
}

// Start I2C communication
void i2c_start() {
    NRF_GPIO->DIRSET = (1 << SDA_PIN) | (1 << SCL_PIN);  // Set SDA and SCL as output

    // Set both SDA and SCL high
    NRF_GPIO->OUTSET = (1 << SDA_PIN) | (1 << SCL_PIN);
    i2c_delay();

    // Pull SDA low
    NRF_GPIO->OUTCLR = (1 << SDA_PIN);
    i2c_delay();

    // Pull SCL low
    NRF_GPIO->OUTCLR = (1 << SCL_PIN);
    i2c_delay();
}

// Stop I2C communication
void i2c_stop() {
    NRF_GPIO->DIRSET = (1 << SDA_PIN) | (1 << SCL_PIN);  // Set SDA and SCL as output

    // Pull SDA low and SCL high
    NRF_GPIO->OUTCLR = (1 << SDA_PIN);
    NRF_GPIO->OUTSET = (1 << SCL_PIN);
    i2c_delay();

    // Pull SDA high
    NRF_GPIO->OUTSET = (1 << SDA_PIN);
    i2c_delay();
}

// Write a bit to SDA line
void i2c_write_bit(bool bit) {
    if (bit)
        NRF_GPIO->OUTSET = (1 << SDA_PIN);  // Set SDA high
    else
        NRF_GPIO->OUTCLR = (1 << SDA_PIN);  // Set SDA low

    NRF_GPIO->OUTSET = (1 << SCL_PIN);    // Set SCL high
    i2c_delay();
    NRF_GPIO->OUTCLR = (1 << SCL_PIN);    // Set SCL low
    i2c_delay();
}

// Read acknowledgment (ACK) from slave
bool i2c_read_ack() {
    bool ack;
    NRF_GPIO->DIRCLR = (1 << SDA_PIN);   // Set SDA as input
    NRF_GPIO->OUTSET = (1 << SCL_PIN);   // Set SCL high
    i2c_delay();
    ack = (NRF_GPIO->IN & (1 << SDA_PIN)) == 0; // ACK is low
    NRF_GPIO->OUTCLR = (1 << SCL_PIN);    // Set SCL low
    NRF_GPIO->DIRSET = (1 << SDA_PIN);   // Set SDA as output
    return ack;
}

// Write a byte to the I2C bus
bool i2c_write_byte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        i2c_write_bit((data & 0x80) != 0);
        data <<= 1;
    }
    return i2c_read_ack();
}

// PCF8574 LCD functions
void lcd_send_nibble(uint8_t nibble) {
    i2c_start();
    i2c_write_byte(LCD_ADDR << 1);  // Send slave address
    i2c_write_byte(nibble | LCD_BACKLIGHT | ENABLE);
    i2c_delay();
    i2c_write_byte(nibble | LCD_BACKLIGHT);
    i2c_stop();
}

void lcd_send_byte(uint8_t byte, bool rs) {
    uint8_t high = (byte & 0xF0);
    uint8_t low  = (byte << 4) & 0xF0;

    lcd_send_nibble(high | (rs ? 0x01 : 0x00));
    lcd_send_nibble(low  | (rs ? 0x01 : 0x00));
}

void lcd_cmd(uint8_t cmd) {
    lcd_send_byte(cmd, false);
    nrf_delay_ms(2);
}

void lcd_write(char c) {
    lcd_send_byte(c, true);
    nrf_delay_ms(2);
}

void lcd_write_string(const char* str) {
    while (*str) lcd_write(*str++);
}

void lcd_init() {
    nrf_delay_ms(50);
    lcd_send_nibble(0x30);
    nrf_delay_ms(5);
    lcd_send_nibble(0x30);
    nrf_delay_us(200);
    lcd_send_nibble(0x30);
    lcd_send_nibble(0x20); // 4-bit

    lcd_cmd(0x28); // 4-bit, 2 line, 5x8
    lcd_cmd(0x0C); // Display on, cursor off
    lcd_cmd(0x06); // Entry mode
    lcd_cmd(0x01); // Clear
    nrf_delay_ms(5);
}

void lcd_set_cursor(uint8_t line) {
    if (line == 1) {
        lcd_cmd(0x80); // Set cursor to the first line (0x80 address)
    } else if (line == 2) {
        lcd_cmd(0xC0); // Set cursor to the second line (0xC0 address)
    }
}

// Function to print on the first line
void lcd_print_first_line(const char *str) {
    lcd_set_cursor(1); // Move the cursor to the first line
    lcd_write_string(str);
}

// Function to print on the second line
void lcd_print_second_line(const char *str) {
    lcd_set_cursor(2); // Move the cursor to the second line
    lcd_write_string(str);
}

int main(void) {
    // Configure GPIO
    NRF_GPIO->DIRSET = (1 << SDA_PIN) | (1 << SCL_PIN); // Set SDA and SCL as output

    lcd_init();

    // Print "Hello" on the first line
    lcd_print_first_line("Hello");

    // Print "World" on the second line
    lcd_print_second_line("World");

    while (1) {
        // Loop forever
    }
}
