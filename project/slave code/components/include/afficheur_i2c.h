#ifndef AFFICHEUR_I2C_H
#define AFFICHEUR_I2C_H

#include "nrf.h"
#include "nrf_delay.h"
#include <stdbool.h>
#include <stdint.h>

// Pin Definitions
#define SDA_PIN         3     // P0.03
#define SCL_PIN         4     // P0.04
#define LCD_ADDR        0x27  // PCF8574 address (change to 0x3F if needed)
#define LCD_BACKLIGHT   0x08  // Backlight control
#define ENABLE          0x04  // Enable bit
#define I2C_TIMEOUT     1000  // Timeout in cycles for I2C operations

void i2c_delay(void);
void i2c_start(void);
void i2c_stop(void);
bool i2c_write_bit(bool bit);
bool i2c_read_ack(void);
bool i2c_write_byte(uint8_t data);
bool lcd_send_nibble(uint8_t nibble);
void lcd_send_byte(uint8_t byte, bool rs);
bool lcd_cmd(uint8_t cmd);
bool lcd_write(char c);
bool lcd_write_string(const char *str);
bool lcd_init(void);
void lcd_set_cursor(uint8_t line);
bool lcd_print_first_line(const char *str);
bool lcd_print_second_line(const char *str);
bool lcd_test(void);

#endif // AFFICHEUR_I2C_H