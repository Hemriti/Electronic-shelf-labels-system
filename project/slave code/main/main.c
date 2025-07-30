#include "bluetooth.h"
#include "custom_nus.h"
#include "nrf.h"
#include "app_timer.h"
#include "SEGGER_RTT.h"
#include "afficheur_i2c.h"

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
    char log[64] = "[ASSERT] Line: ";
    char line_str[5];
    uint32_to_hex(line_num, line_str, 4);
    strcat_safe(log, line_str, sizeof(log));
    strcat_safe(log, ", File: ", sizeof(log));
    strcat_safe(log, (const char *)p_file_name, sizeof(log));
    strcat_safe(log, "\n", sizeof(log));
    SEGGER_RTT_WriteString(0, log);
    while (1);
}

int main(void) {
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "[INIT] Peripheral Start\n");

    // Configure GPIO for LCD
    NRF_GPIO->DIRSET = (1 << SDA_PIN) | (1 << SCL_PIN);
    NRF_GPIO->OUTSET = (1 << SDA_PIN) | (1 << SCL_PIN);

    // Initialize LCD but do not display anything
    if (!lcd_init()) {
        SEGGER_RTT_WriteString(0, "[INIT] LCD Init failed\n");
    } else {
        SEGGER_RTT_WriteString(0, "[INIT] LCD Initialized\n");
    }
bluetooth_init();
    bluetooth_advertising_start();

    for (;;) {
        sd_app_evt_wait();
    }
}