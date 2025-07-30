#include "custom_nus.h"
#include "bluetooth.h"
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "SEGGER_RTT.h"
#include "nrf_delay.h"
#include "afficheur_i2c.h"
#include <string.h>

static ble_nus_t m_nus;
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t m_nus_uuid_type;
static const ble_uuid128_t nus_base_uuid = {
    .uuid128 = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
                0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E}
};
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, 0}};
static packet_t packet_queue[PACKET_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;
static uint8_t queue_count = 0;
static receive_state_t receive_state = RECEIVE_STATE_IDLE;
static char received_id[8];
static char received_name[16];
static char received_price[8];
static char received_details[16];

void strcat_safe(char *dest, const char *src, uint32_t max_len) {
    uint32_t dest_len = strlen(dest);
    uint32_t src_len = strlen(src);
    uint32_t copy_len = (dest_len + src_len < max_len - 1) ? src_len : (max_len - 1 - dest_len);
    if (copy_len > 0) {
        strncat(dest, src, copy_len);
        dest[max_len - 1] = '\0';
    }
}

void uint32_to_hex(uint32_t value, char *buf, uint8_t digits) {
    const char hex[] = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) {
        buf[i] = hex[value & 0xF];
        value >>= 4;
    }
    buf[digits] = '\0';
}

static void lcd_display_product(void) {
    char line1[17] = {0};
    char line2[17] = {0};
    if (strlen(received_name) == 0) strcpy(received_name, "NoName");
    if (strlen(received_price) == 0) strcpy(received_price, "0");
    if (strlen(received_details) == 0) strcpy(received_details, "NoDetails");
    strcat_safe(line1, received_name, sizeof(line1));
    strcat_safe(line1, " ", sizeof(line1));
    strcat_safe(line1, received_details, sizeof(line1));
    strcat_safe(line2, received_price, sizeof(line2));
    strcat_safe(line2, " dtn", sizeof(line2));
    SEGGER_RTT_WriteString(0, "[LCD] Updating: ");
    SEGGER_RTT_WriteString(0, line1);
    SEGGER_RTT_WriteString(0, ", ");
    SEGGER_RTT_WriteString(0, line2);
    SEGGER_RTT_WriteString(0, "\n");
    if (!lcd_init()) {
        SEGGER_RTT_WriteString(0, "[LCD] Reinit failed\n");
        return;
    }
    if (!lcd_cmd(0x01)) {
        SEGGER_RTT_WriteString(0, "[LCD] Clear failed\n");
        return;
    }
    nrf_delay_ms(2);
    if (!lcd_print_first_line(line1)) {
        SEGGER_RTT_WriteString(0, "[LCD] Line1 failed\n");
        return;
    }
    if (!lcd_print_second_line(line2)) {
        SEGGER_RTT_WriteString(0, "[LCD] Line2 failed\n");
        return;
    }
    SEGGER_RTT_WriteString(0, "[LCD] Update complete\n");
}

void custom_nus_send_string(const char *str) {
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) return;
    uint32_t err_code = ble_nus_string_send(&m_nus, (uint8_t *)str, strlen(str));
    if (err_code != NRF_SUCCESS) {
        char log[32] = "[TX] Failed: ";
        char hex[5];
        uint32_to_hex(err_code, hex, 4);
        strcat_safe(log, hex, sizeof(log));
        strcat_safe(log, "\n", sizeof(log));
        SEGGER_RTT_WriteString(0, log);
    } else {
        char log[32] = "[TX] ";
        strcat_safe(log, str, sizeof(log));
        strcat_safe(log, "\n", sizeof(log));
        SEGGER_RTT_WriteString(0, log);
    }
}

bool custom_nus_enqueue_packet(uint8_t *p_data, uint16_t length) {
    if (queue_count >= PACKET_QUEUE_SIZE || length == 0 || length > MAX_PACKET_LEN || p_data == NULL) {
        SEGGER_RTT_WriteString(0, "[QUEUE] Invalid\n");
        return false;
    }
    memcpy(packet_queue[queue_tail].data, p_data, length);
    packet_queue[queue_tail].length = length;
    queue_tail = (queue_tail + 1) % PACKET_QUEUE_SIZE;
    queue_count++;
    char log[32] = "[QUEUE] Enqueued, count: ";
    char count_str[3];
    uint32_to_hex(queue_count, count_str, 2);
    strcat_safe(log, count_str, sizeof(log));
    strcat_safe(log, "\n", sizeof(log));
    SEGGER_RTT_WriteString(0, log);
    return true;
}

bool custom_nus_dequeue_packet(uint8_t *p_data, uint16_t *p_length) {
    if (queue_count == 0) return false;
    memcpy(p_data, packet_queue[queue_head].data, packet_queue[queue_head].length);
    *p_length = packet_queue[queue_head].length;
    queue_head = (queue_head + 1) % PACKET_QUEUE_SIZE;
    queue_count--;
    return true;
}

void custom_nus_process_packet(uint8_t *p_data, uint16_t length) {
    char buffer[MAX_PACKET_LEN + 1];
    if (length == 0 || length > MAX_PACKET_LEN || p_data == NULL) return;
    memcpy(buffer, p_data, length);
    buffer[length] = '\0';
    if (strcmp(buffer, "TEST") == 0) {
        lcd_test();
        custom_nus_send_string("TEST_OK");
        return;
    }
    switch (receive_state) {
        case RECEIVE_STATE_ID:
            strncpy(received_id, buffer, sizeof(received_id) - 1);
            received_id[sizeof(received_id) - 1] = '\0';
            SEGGER_RTT_WriteString(0, "[RX] ID: ");
            SEGGER_RTT_WriteString(0, received_id);
            SEGGER_RTT_WriteString(0, "\n");
            custom_nus_send_string("1");
            receive_state = RECEIVE_STATE_NAME;
            break;
        case RECEIVE_STATE_NAME:
            if (strlen(buffer) >= sizeof(received_name)) return;
            strncpy(received_name, buffer, sizeof(received_name) - 1);
            received_name[sizeof(received_name) - 1] = '\0';
            SEGGER_RTT_WriteString(0, "[RX] Name: ");
            SEGGER_RTT_WriteString(0, received_name);
            SEGGER_RTT_WriteString(0, "\n");
            custom_nus_send_string("2");
            receive_state = RECEIVE_STATE_PRICE;
            break;
        case RECEIVE_STATE_PRICE:
            if (strlen(buffer) >= sizeof(received_price)) return;
            strncpy(received_price, buffer, sizeof(received_price) - 1);
            received_price[sizeof(received_price) - 1] = '\0';
            SEGGER_RTT_WriteString(0, "[RX] Price: ");
            SEGGER_RTT_WriteString(0, received_price);
            SEGGER_RTT_WriteString(0, "\n");
            custom_nus_send_string("3");
            receive_state = RECEIVE_STATE_DETAILS;
            break;
        case RECEIVE_STATE_DETAILS:
            if (strlen(buffer) >= sizeof(received_details)) return;
            strncpy(received_details, buffer, sizeof(received_details) - 1);
            received_details[sizeof(received_details) - 1] = '\0';
            SEGGER_RTT_WriteString(0, "[RX] Details: ");
            SEGGER_RTT_WriteString(0, received_details);
            SEGGER_RTT_WriteString(0, "\n");
            SEGGER_RTT_WriteString(0, received_name);
            SEGGER_RTT_WriteString(0, " ");
            SEGGER_RTT_WriteString(0, received_details);
            SEGGER_RTT_WriteString(0, "\n");
            custom_nus_send_string("4");
            receive_state = RECEIVE_STATE_IDLE;
            lcd_display_product();
            sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            break;
        default:
            break;
    }
}

static void nus_data_handler(ble_nus_t *p_nus, uint8_t *p_data, uint16_t length) {
    if (!custom_nus_enqueue_packet(p_data, length)) return;
    uint8_t packet_data[MAX_PACKET_LEN];
    uint16_t packet_length;
    if (custom_nus_dequeue_packet(packet_data, &packet_length)) {
        custom_nus_process_packet(packet_data, packet_length);
    }
}

void custom_nus_services_init(void) {
    uint32_t err_code;
    ble_nus_init_t nus_init;
    err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &m_nus_uuid_type);
    APP_ERROR_CHECK(err_code);
    memset(&nus_init, 0, sizeof(nus_init));
    nus_init.data_handler = nus_data_handler;
    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
    m_adv_uuids[0].type = m_nus_uuid_type;
}

void custom_nus_on_ble_evt(ble_evt_t *p_ble_evt) {
    char log[32];
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            receive_state = RECEIVE_STATE_ID;
            memset(received_id, 0, sizeof(received_id));
            memset(received_name, 0, sizeof(received_name));
            memset(received_price, 0, sizeof(received_price));
            memset(received_details, 0, sizeof(received_details));
            queue_head = 0;
            queue_tail = 0;
            queue_count = 0;
            strcpy(log, "Connected, handle: ");
            uint32_to_hex(m_conn_handle, log + strlen(log), 4);
            strcat_safe(log, "\n", sizeof(log));
            SEGGER_RTT_WriteString(0, log);
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            receive_state = RECEIVE_STATE_IDLE;
            queue_head = 0;
            queue_tail = 0;
            queue_count = 0;
            SEGGER_RTT_WriteString(0, "[BLE] Disconnected\n");
            bluetooth_on_disconnect();
            break;
        case BLE_GATTS_EVT_WRITE:
            if (p_ble_evt->evt.gatts_evt.params.write.handle == m_nus.rx_handles.cccd_handle) {
                strcpy(log, "CCCD write, value: ");
                uint32_to_hex(p_ble_evt->evt.gatts_evt.params.write.data[0], log + strlen(log), 4);
                strcat_safe(log, "\n", sizeof(log));
                SEGGER_RTT_WriteString(0, log);
            }
            break;
        default:
            break;
    }
    ble_nus_on_ble_evt(&m_nus, p_ble_evt);
}

ble_uuid_t *custom_nus_get_adv_uuids(void) {
    return m_adv_uuids;
}