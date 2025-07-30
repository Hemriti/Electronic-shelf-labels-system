#include "afficheur_i2c.h"
#include "SEGGER_RTT.h"

// Reinitialize GPIO pins
static void i2c_gpio_init(void) {
    NRF_GPIO->DIRSET = (1 << SDA_PIN) | (1 << SCL_PIN);
    NRF_GPIO->OUTSET = (1 << SDA_PIN) | (1 << SCL_PIN);
}

// I2C communication delay
void i2c_delay() {
    nrf_delay_us(5); // Short for SoftDevice compatibility
}

// Start I2C communication
void i2c_start() {
    i2c_gpio_init();
    NRF_GPIO->OUTSET = (1 << SDA_PIN) | (1 << SCL_PIN);
    i2c_delay();
    NRF_GPIO->OUTCLR = (1 << SDA_PIN);
    i2c_delay();
    NRF_GPIO->OUTCLR = (1 << SCL_PIN);
    i2c_delay();
}

// Stop I2C communication
void i2c_stop() {
    i2c_gpio_init();
    NRF_GPIO->OUTCLR = (1 << SDA_PIN);
    NRF_GPIO->OUTSET = (1 << SCL_PIN);
    i2c_delay();
    NRF_GPIO->OUTSET = (1 << SDA_PIN);
    i2c_delay();
}

// Write a bit to SDA line
bool i2c_write_bit(bool bit) {
    if (bit)
        NRF_GPIO->OUTSET = (1 << SDA_PIN);
    else
        NRF_GPIO->OUTCLR = (1 << SDA_PIN);
    NRF_GPIO->OUTSET = (1 << SCL_PIN);
    i2c_delay();
    NRF_GPIO->OUTCLR = (1 << SCL_PIN);
    i2c_delay();
    return true;
}

// Read acknowledgment (ACK) from slave
bool i2c_read_ack() {
    bool ack;
    NRF_GPIO->DIRCLR = (1 << SDA_PIN);
    NRF_GPIO->OUTSET = (1 << SCL_PIN);
    i2c_delay();
    ack = (NRF_GPIO->IN & (1 << SDA_PIN)) == 0;
    NRF_GPIO->OUTCLR = (1 << SCL_PIN);
    NRF_GPIO->DIRSET = (1 << SDA_PIN);
    if (!ack) {
        SEGGER_RTT_WriteString(0, "[I2C] No ACK\n");
    }
    return ack;
}

// Write a byte to the I2C bus
bool i2c_write_byte(uint8_t data) {
    uint32_t timeout = I2C_TIMEOUT;
    for (int i = 0; i < 8; i++) {
        if (!i2c_write_bit((data & 0x80) != 0)) {
            SEGGER_RTT_WriteString(0, "[I2C] Write bit failed\n");
            return false;
        }
        data <<= 1;
        if (--timeout == 0) {
            SEGGER_RTT_WriteString(0, "[I2C] Write timeout\n");
            return false;
        }
    }
    return i2c_read_ack();
}

// PCF8574 LCD functions
bool lcd_send_nibble(uint8_t nibble) {
    i2c_start();
    if (!i2c_write_byte(LCD_ADDR << 1)) {
        SEGGER_RTT_WriteString(0, "[LCD] Address fail\n");
        i2c_stop();
        return false;
    }
    if (!i2c_write_byte(nibble | LCD_BACKLIGHT | ENABLE)) {
        SEGGER_RTT_WriteString(0, "[LCD] Nibble (enable) fail\n");
        i2c_stop();
        return false;
    }
    i2c_delay();
    if (!i2c_write_byte(nibble | LCD_BACKLIGHT)) {
        SEGGER_RTT_WriteString(0, "[LCD] Nibble fail\n");
        i2c_stop();
        return false;
    }
    i2c_stop();
    return true;
}

void lcd_send_byte(uint8_t byte, bool rs) {
    uint8_t high = (byte & 0xF0);
    uint8_t low  = (byte << 4) & 0xF0;
    lcd_send_nibble(high | (rs ? 0x01 : 0x00));
    lcd_send_nibble(low  | (rs ? 0x01 : 0x00));
}

bool lcd_cmd(uint8_t cmd) {
    lcd_send_byte(cmd, false);
    nrf_delay_us(200);
    return true;
}

bool lcd_write(char c) {
    lcd_send_byte(c, true);
    nrf_delay_us(200);
    return true;
}

bool lcd_write_string(const char* str) {
    uint8_t count = 0;
    while (*str && count < 16) {
        if (!lcd_write(*str++)) {
            SEGGER_RTT_WriteString(0, "[LCD] Write string failed\n");
            return false;
        }
        count++;
    }
    return true;
}

bool lcd_init() {
    nrf_delay_ms(50);
    if (!lcd_send_nibble(0x30)) return false;
    nrf_delay_ms(5);
    if (!lcd_send_nibble(0x30)) return false;
    nrf_delay_us(200);
    if (!lcd_send_nibble(0x30)) return false;
    if (!lcd_send_nibble(0x20)) return false; // 4-bit
    if (!lcd_cmd(0x28)) return false; // 4-bit, 2 line, 5x8
    if (!lcd_cmd(0x0C)) return false; // Display on, cursor off
    if (!lcd_cmd(0x06)) return false; // Entry mode
    if (!lcd_cmd(0x01)) return false; // Clear
    nrf_delay_ms(2);
    return true;
}

void lcd_set_cursor(uint8_t line) {
    if (line == 1) {
        lcd_cmd(0x80); // First line
    } else if (line == 2) {
        lcd_cmd(0xC0); // Second line
    }
}

bool lcd_print_first_line(const char *str) {
    lcd_set_cursor(1);
    return lcd_write_string(str);
}

bool lcd_print_second_line(const char *str) {
    lcd_set_cursor(2);
    return lcd_write_string(str);
}

bool lcd_test() {
    SEGGER_RTT_WriteString(0, "[LCD] Test start\n");
    if (!lcd_init()) {
        SEGGER_RTT_WriteString(0, "[LCD] Test init failed\n");
        return false;
    }
    if (!lcd_cmd(0x01)) {
        SEGGER_RTT_WriteString(0, "[LCD] Test clear failed\n");
        return false;
    }
    nrf_delay_ms(2);
    if (!lcd_print_first_line("LCD Test")) {
        SEGGER_RTT_WriteString(0, "[LCD] Test line1 failed\n");
        return false;
    }
    if (!lcd_print_second_line("OK")) {
        SEGGER_RTT_WriteString(0, "[LCD] Test line2 failed\n");
        return false;
    }
    SEGGER_RTT_WriteString(0, "[LCD] Test complete\n");
    return true;
}