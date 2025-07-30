//#include "bluetooth.h"
//#include "nus.h"
//static bool nus_ready = false;

//static const ble_uuid128_t BLE_NUS_BASE_UUID = {
//    {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
//     0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E}
//};
//typedef enum {
//    STATE_IDLE,           // Initial state, waiting to send "1111"
//    STATE_SENDING_1111,   // Sending "1111", waiting for "done"
//    STATE_WAITING_1,      // Waiting for "1" to send "laptop"
//    STATE_WAITING_2,      // Waiting for "2" to send "1500"
//    STATE_WAITING_3,      // Waiting for "3" to send "lenovo"
//    STATE_WAITING_4       // Waiting for "4" to disconnect
//} comm_state_t;





//static void send_nus_string(const char *str) {
//    if (!nus_ready) {
//        SEGGER_RTT_WriteString(0, "[TX] NUS not ready, cannot send\n");
//        return;
//    }
//    uint32_t err_code = ble_nus_c_string_send(&m_ble_nus_c, (uint8_t *)str, strlen(str));
//    if (err_code == NRF_SUCCESS) {
//        char log[32];
//        snprintf(log, sizeof(log), "[TX] %s\n", str);
//        SEGGER_RTT_WriteString(0, log);
//    } else {
//        char log[32];
//        snprintf(log, sizeof(log), "[TX failed: %u]\n", err_code);
//        SEGGER_RTT_WriteString(0, log);
//    }
//}

//static void send_hello(void *p_context) {
//    if (!nus_ready) {
//        SEGGER_RTT_WriteString(0, "[TIMER] NUS not ready, skipping send\n");
//        return;
//    }
//    if (comm_state == STATE_IDLE) {
//        send_nus_string("1111");
//        comm_state = STATE_SENDING_1111;
//        SEGGER_RTT_WriteString(0, "[STATE] Transition to SENDING_1111\n");
//    }
//}


//static void ble_nus_c_evt_handler(ble_nus_c_t *p_ble_nus_c, const ble_nus_c_evt_t *p_ble_nus_evt) {
//    uint32_t err_code;
//    switch (p_ble_nus_evt->evt_type) {
//        case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
//            ble_nus_c_handles_assign(p_ble_nus_c, p_ble_nus_evt->conn_handle, &p_ble_nus_evt->handles);
//            err_code = ble_nus_c_rx_notif_enable(p_ble_nus_c);
//            if (err_code == NRF_SUCCESS) {
//                nus_ready = true;
//                SEGGER_RTT_WriteString(0, "NUS discovery complete, notifications enabled\n");
//                err_code = app_timer_start(m_msg_timer_id, MSG_INTERVAL, NULL);
//                if (err_code == NRF_SUCCESS) {
//                    SEGGER_RTT_WriteString(0, "Timer started for sending '1111'\n");
//                } else {
//                    char log[32];
//                    snprintf(log, sizeof(log), "Timer start failed: %u\n", err_code);
//                    SEGGER_RTT_WriteString(0, log);
//                }
//            } else {
//                char log[32];
//                snprintf(log, sizeof(log), "Failed to enable notifications: %u\n", err_code);
//                SEGGER_RTT_WriteString(0, log);
//            }
//            break;
//        case BLE_NUS_C_EVT_NUS_RX_EVT: {
//            char log[64];
//            char hex[32];
//            char ascii[16];
//            uint32_t hex_pos = 0;
//            uint32_t ascii_pos = 0;

//            for (uint32_t i = 0; i < p_ble_nus_evt->data_len && hex_pos < sizeof(hex) - 4 && ascii_pos < sizeof(ascii) - 1; i++) {
//                hex_pos += snprintf(hex + hex_pos, sizeof(hex) - hex_pos, "%02X ", p_ble_nus_evt->p_data[i]);
//                ascii[ascii_pos++] = (p_ble_nus_evt->p_data[i] >= 32 && p_ble_nus_evt->p_data[i] <= 126) ? p_ble_nus_evt->p_data[i] : '.';
//            }
//            ascii[ascii_pos] = '\0';
//            snprintf(log, sizeof(log), "[RX] Length: %u, Data: %s(ASCII: %s)\n", p_ble_nus_evt->data_len, hex, ascii);
//            SEGGER_RTT_WriteString(0, log);

//            if (comm_state == STATE_SENDING_1111 && p_ble_nus_evt->data_len == 4 &&
//                memcmp(p_ble_nus_evt->p_data, "done", 4) == 0) {
//                send_nus_string("laptop");
//                comm_state = STATE_WAITING_1;
//                SEGGER_RTT_WriteString(0, "[STATE] Transition to WAITING_1\n");
//            }
//            else if (comm_state == STATE_WAITING_1 && p_ble_nus_evt->data_len == 1 &&
//                     p_ble_nus_evt->p_data[0] == '1') {
//                send_nus_string("1500");
//                comm_state = STATE_WAITING_2;
//                nrf_gpio_pin_set(LED_PIN);
//                SEGGER_RTT_WriteString(0, "[STATE] Transition to WAITING_2, LED ON\n");
//            }
//            else if (comm_state == STATE_WAITING_2 && p_ble_nus_evt->data_len == 1 &&
//                     p_ble_nus_evt->p_data[0] == '2') {
//                send_nus_string("lenovo");
//                comm_state = STATE_WAITING_3;
//                SEGGER_RTT_WriteString(0, "[STATE] Transition to WAITING_3\n");
//            }
//            else if (comm_state == STATE_WAITING_3 && p_ble_nus_evt->data_len == 1 &&
//                     p_ble_nus_evt->p_data[0] == '3') {
//                comm_state = STATE_WAITING_4;
//                SEGGER_RTT_WriteString(0, "[STATE] Transition to WAITING_4\n");
//            }
//            else if (comm_state == STATE_WAITING_4 && p_ble_nus_evt->data_len == 1 &&
//                     p_ble_nus_evt->p_data[0] == '4') {
//                SEGGER_RTT_WriteString(0, "Received '4', disconnecting\n");
//                err_code = sd_ble_gap_disconnect(p_ble_nus_c->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
//                if (err_code != NRF_SUCCESS) {
//                    char log[32];
//                    snprintf(log, sizeof(log), "Disconnect failed: %u\n", err_code);
//                    SEGGER_RTT_WriteString(0, log);
//                }
//                comm_state = STATE_IDLE;
//                SEGGER_RTT_WriteString(0, "[STATE] Transition to IDLE\n");
//            }
//            else {
//                char log[32];
//                snprintf(log, sizeof(log), "[ERROR] Unexpected data in state %u\n", comm_state);
//                SEGGER_RTT_WriteString(0, log);
//                comm_state = STATE_IDLE;
//                SEGGER_RTT_WriteString(0, "[STATE] Reset to IDLE due to unexpected data\n");
//            }
//            break;
//        }
//        case BLE_NUS_C_EVT_DISCONNECTED:
//            SEGGER_RTT_WriteString(0, "Disconnected\n");
//            nus_ready = false;
//            comm_state = STATE_IDLE;
//            scan_start();
//            nrf_gpio_pin_clear(LED_PIN);
//            SEGGER_RTT_WriteString(0, "[STATE] Transition to IDLE, LED OFF\n");
//            break;
//        default:
//            break;
//    }
//}


//static void timers_init(void) {
//    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
//    uint32_t err_code = app_timer_create(&m_msg_timer_id, APP_TIMER_MODE_REPEATED, send_hello);
//    if (err_code != NRF_SUCCESS) {
//        char log[32];
//        snprintf(log, sizeof(log), "Timer create failed: %u\n", err_code);
//        SEGGER_RTT_WriteString(0, log);
//    }
//}

//static void nus_c_init(void) {
//    ble_nus_c_init_t init = {0};
//    init.evt_handler = ble_nus_c_evt_handler;
//    uint32_t err_code = ble_nus_c_init(&m_ble_nus_c, &init);
//    if (err_code != NRF_SUCCESS) {
//        char log[32];
//        snprintf(log, sizeof(log), "NUS client init failed: %u\n", err_code);
//        SEGGER_RTT_WriteString(0, log);
//    }
//}
