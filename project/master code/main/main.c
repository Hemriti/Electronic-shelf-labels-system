#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include "softdevice_handler.h"
#include "SEGGER_RTT.h"
#include "ble_db_discovery.h"
#include "ble_nus_c.h"
#include "app_timer.h"
#include "nrf_sdm.h"
#include "nrf_clock.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_drv_gpiote.h"
#include "app_uart.h"
#include "nrf_uart.h"

// BLE Central parameters
#define CENTRAL_LINK_COUNT      1
#define PERIPHERAL_LINK_COUNT   0
#define APP_TIMER_PRESCALER     0
#define APP_TIMER_OP_QUEUE_SIZE 2
#define NRF_BLE_MAX_MTU_SIZE    23
#define SCAN_INTERVAL           0x00A0
#define SCAN_WINDOW             0x0050
#define SCAN_TIMEOUT            0x0064
#define MIN_CONNECTION_INTERVAL MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONNECTION_INTERVAL MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define SLAVE_LATENCY           0
#define SUPERVISION_TIMEOUT     MSEC_TO_UNITS(4000, UNIT_10_MS)
#define LED_PIN                 21
#define SWITCH_PIN              22
#define UART_TX_PIN             2
#define UART_RX_PIN             3

// Product list parameters
#define MAX_PRODUCTS            10
typedef struct {
    ble_gap_addr_t addr;
    char id[8];
    char name[16];
    char price[8];
    char details[16];
} product_t;
static product_t product_list[MAX_PRODUCTS];
static uint32_t product_count = 0;
static uint32_t current_product_idx = 0;

// Flash storage configuration
#define FLASH_PAGE_ADDR         0x3E000 // Last page for nRF51822 (256 KB)
static bool products_stored = false;

// UART configuration
#define UART_BUFFER_SIZE        256
static uint8_t uart_rx_buffer[UART_BUFFER_SIZE];
static uint8_t uart_tx_buffer[UART_BUFFER_SIZE];
static uint16_t buffer_index = 0;

// UART reception state
static bool uart_receiving = true;
typedef enum {
    UART_STATE_MAC,
    UART_STATE_ID,
    UART_STATE_NAME,
    UART_STATE_PRICE,
    UART_STATE_DETAILS,
    UART_STATE_COMMAND
} uart_state_t;
static uart_state_t uart_state = UART_STATE_MAC;
static product_t temp_product;

// Store the connected peripheral's address
static ble_gap_addr_t connected_peer_addr;

// Send state for part-by-part transfer
typedef enum {
    SEND_STATE_IDLE,
    SEND_STATE_ID,
    SEND_STATE_NAME,
    SEND_STATE_PRICE,
    SEND_STATE_DETAILS,
    SEND_STATE_DONE
} send_state_t;
static send_state_t send_state = SEND_STATE_IDLE;

static ble_db_discovery_t m_ble_db_discovery;
static ble_nus_c_t m_ble_nus_c;
static bool nus_ready = false;
static uint8_t nus_uuid_type;
static bool scanning = false;

static const ble_uuid128_t BLE_NUS_BASE_UUID = {
    {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
     0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E}
};

static ble_gap_scan_params_t m_scan_params = {
    .active   = 1,
    .interval = SCAN_INTERVAL,
    .window   = SCAN_WINDOW,
    .timeout  = SCAN_TIMEOUT,
    .selective = 0
};

static const ble_gap_conn_params_t m_connection_param = {
    .min_conn_interval = (uint16_t)MIN_CONNECTION_INTERVAL,
    .max_conn_interval = (uint16_t)MAX_CONNECTION_INTERVAL,
    .slave_latency     = (uint16_t)SLAVE_LATENCY,
    .conn_sup_timeout  = (uint16_t)SUPERVISION_TIMEOUT
};

static ble_uuid_t m_target_uuid = {
    .uuid = BLE_UUID_NUS_SERVICE,
    .type = 0
};

// Function prototypes
static void uart_event_handler(app_uart_evt_t *p_event);
static void switch_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
static void scan_start(void);
static void gpio_init(void);
static void uart_init(void);
static void send_nus_string(const char *str);
static bool is_uuid_present(const ble_uuid_t *p_target_uuid, const ble_gap_evt_adv_report_t *p_adv_report);
static void ble_nus_c_evt_handler(ble_nus_c_t *p_ble_nus_c, const ble_nus_c_evt_t *p_ble_nus_evt);
static void db_disc_handler(ble_db_discovery_evt_t *p_evt);
static bool is_address_in_list(const ble_gap_addr_t *scanned_addr);
static void ble_evt_handler(ble_evt_t *p_ble_evt);
static void ble_evt_dispatch(ble_evt_t *p_ble_evt);
static void log_central_address(void);
static void ble_stack_init(void);
static void timers_init(void);
static bool parse_mac_address(const char *mac_str, ble_gap_addr_t *addr);
static void nus_c_init(void);
static void flash_page_erase(uint32_t *page_address);
static void flash_word_write(uint32_t *address, uint32_t value);
static void read_products_from_flash(void);
static void write_products_to_flash(void);
static void erase_flash(void);
static void remove_product_from_list(uint32_t idx);

static void gpio_init(void) {
    nrf_gpio_cfg_output(LED_PIN);
    nrf_gpio_pin_clear(LED_PIN);
    
    nrf_gpio_cfg_input(SWITCH_PIN, NRF_GPIO_PIN_PULLUP);
    
    if (!nrf_drv_gpiote_is_init()) {
        uint32_t err_code = nrf_drv_gpiote_init();
        APP_ERROR_CHECK(err_code);
    }
    
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
    in_config.pull = NRF_GPIO_PIN_PULLUP;
    
    uint32_t err_code = nrf_drv_gpiote_in_init(SWITCH_PIN, &in_config, switch_handler);
    APP_ERROR_CHECK(err_code);
    
    nrf_drv_gpiote_in_event_enable(SWITCH_PIN, true);
}

static void uart_init(void) {
    uint32_t err_code;
    
    const app_uart_comm_params_t comm_params = {
        .rx_pin_no    = UART_RX_PIN,
        .tx_pin_no    = UART_TX_PIN,
        .rts_pin_no   = 0,
        .cts_pin_no   = 0,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud115200
    };

    app_uart_buffers_t buffers = {
        .rx_buf       = uart_rx_buffer,
        .rx_buf_size  = UART_BUFFER_SIZE,
        .tx_buf       = uart_tx_buffer,
        .tx_buf_size  = UART_BUFFER_SIZE
    };

    err_code = app_uart_init(&comm_params, &buffers, uart_event_handler, APP_IRQ_PRIORITY_LOW);
    if (err_code != NRF_SUCCESS) {
        SEGGER_RTT_WriteString(0, "UART init failed\n");
        while (true) { /* Infinite loop on error */ }
    }
}

static void switch_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    if (products_stored && product_count > 0) {
        uart_receiving = false;
        scanning = true;
        SEGGER_RTT_WriteString(0, "Switch toggled, stopping UART reception, starting BLE scan\n");
        app_uart_flush();
        scan_start();
    } else {
        SEGGER_RTT_WriteString(0, "No products in flash, ignoring switch toggle\n");
    }
}

static void send_nus_string(const char *str) {
    if (!nus_ready) {
        SEGGER_RTT_WriteString(0, "[TX] NUS not ready, cannot send\n");
        return;
    }
    uint32_t err_code = ble_nus_c_string_send(&m_ble_nus_c, (uint8_t *)str, strlen(str));
    if (err_code == NRF_SUCCESS) {
        char log[64];
        snprintf(log, sizeof(log), "[TX] %s\n", str);
        SEGGER_RTT_WriteString(0, log);
    } else {
        char log[64];
        if (err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING) {
            snprintf(log, sizeof(log), "[TX failed: Notifications not enabled]\n");
        } else if (err_code == NRF_ERROR_INVALID_STATE) {
            snprintf(log, sizeof(log), "[TX failed: Invalid state, NUS not discovered]\n");
        } else {
            snprintf(log, sizeof(log), "[TX failed: %u]\n", err_code);
        }
        SEGGER_RTT_WriteString(0, log);
    }
}

static void scan_start(void) {
    if (!scanning) return;
    uint32_t err_code = sd_ble_gap_scan_start(&m_scan_params);
    if (err_code == NRF_SUCCESS) {
        SEGGER_RTT_WriteString(0, "Scanning started...\n");
    } else {
        char log[32];
        snprintf(log, sizeof(log), "Scan start failed: %u\n", err_code);
        SEGGER_RTT_WriteString(0, log);
    }
}

static bool is_uuid_present(const ble_uuid_t *p_target_uuid, const ble_gap_evt_adv_report_t *p_adv_report) {
    uint32_t index = 0;
    uint8_t *p_data = (uint8_t *)p_adv_report->data;

    while (index < p_adv_report->dlen) {
        uint8_t length = p_data[index];
        uint8_t type   = p_data[index + 1];

        if ((type == BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE) ||
            (type == BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE)) {
            if (memcmp(&p_data[index + 2], BLE_NUS_BASE_UUID.uuid128, 16) == 0) {
                return true;
            }
        }
        index += length + 1;
    }
    return false;
}

static void ble_nus_c_evt_handler(ble_nus_c_t *p_ble_nus_c, const ble_nus_c_evt_t *p_ble_nus_evt) {
    uint32_t err_code;
    switch (p_ble_nus_evt->evt_type) {
        case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
            ble_nus_c_handles_assign(p_ble_nus_c, p_ble_nus_evt->conn_handle, &p_ble_nus_evt->handles);
            err_code = ble_nus_c_rx_notif_enable(p_ble_nus_c);
            if (err_code == NRF_SUCCESS) {
                nus_ready = true;
                SEGGER_RTT_WriteString(0, "NUS discovery complete, notifications enabled\n");
                for (uint32_t i = 0; i < product_count; i++) {
                    if (memcmp(connected_peer_addr.addr, product_list[i].addr.addr, BLE_GAP_ADDR_LEN) == 0 &&
                        connected_peer_addr.addr_type == product_list[i].addr.addr_type) {
                        current_product_idx = i;
                        send_state = SEND_STATE_ID;
                        send_nus_string(product_list[i].id);
                        break;
                    }
                }
            } else {
                char log[32];
                snprintf(log, sizeof(log), "Failed to enable notifications: %u\n", err_code);
                SEGGER_RTT_WriteString(0, log);
            }
            break;
        case BLE_NUS_C_EVT_NUS_RX_EVT: {
            char log[64];
            char hex[32];
            char ascii[16];
            uint32_t hex_pos = 0;
            uint32_t ascii_pos = 0;

            for (uint32_t i = 0; i < p_ble_nus_evt->data_len && hex_pos < sizeof(hex) - 4 && ascii_pos < sizeof(ascii) - 1; i++) {
                hex_pos += snprintf(hex + hex_pos, sizeof(hex) - hex_pos, "%02X ", p_ble_nus_evt->p_data[i]);
                ascii[ascii_pos++] = (p_ble_nus_evt->p_data[i] >= 32 && p_ble_nus_evt->p_data[i] <= 126) ? p_ble_nus_evt->p_data[i] : '.';
            }
            ascii[ascii_pos] = '\0';
            snprintf(log, sizeof(log), "[RX] Length: %u, Data: %s(ASCII: %s)\n", p_ble_nus_evt->data_len, hex, ascii);
            SEGGER_RTT_WriteString(0, log);

            if (p_ble_nus_evt->data_len == 1) {
                if (send_state == SEND_STATE_ID && p_ble_nus_evt->p_data[0] == '1') {
                    send_state = SEND_STATE_NAME;
                    send_nus_string(product_list[current_product_idx].name);
                } else if (send_state == SEND_STATE_NAME && p_ble_nus_evt->p_data[0] == '2') {
                    send_state = SEND_STATE_PRICE;
                    send_nus_string(product_list[current_product_idx].price);
                } else if (send_state == SEND_STATE_PRICE && p_ble_nus_evt->p_data[0] == '3') {
                    send_state = SEND_STATE_DETAILS;
                    send_nus_string(product_list[current_product_idx].details);
                } else if (send_state == SEND_STATE_DETAILS && p_ble_nus_evt->p_data[0] == '4') {
                    send_state = SEND_STATE_DONE;
                    scanning = false;
                    SEGGER_RTT_WriteString(0, "Product sent successfully\n");
                    err_code = sd_ble_gap_disconnect(p_ble_nus_c->conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                    if (err_code != NRF_SUCCESS) {
                        snprintf(log, sizeof(log), "Disconnect failed: %u\n", err_code);
                        SEGGER_RTT_WriteString(0, log);
                    }
                    // Remove product from list and update flash
                    remove_product_from_list(current_product_idx);
                    if (product_count > 0) {
                        scanning = true;
                        scan_start();
                    } else {
                        products_stored = false;
                        uart_receiving = true;
                        SEGGER_RTT_WriteString(0, "All products sent, resuming UART reception\n");
                    }
                }
            }
            break;
        }
        case BLE_NUS_C_EVT_DISCONNECTED:
            SEGGER_RTT_WriteString(0, "Disconnected\n");
            nus_ready = false;
            send_state = SEND_STATE_IDLE;
            if (product_count > 0) {
                scan_start();
            }
            nrf_gpio_pin_clear(LED_PIN);
            break;
        default:
            break;
    }
}

static void db_disc_handler(ble_db_discovery_evt_t *p_evt) {
    ble_nus_c_on_db_disc_evt(&m_ble_nus_c, p_evt);
}

static bool is_address_in_list(const ble_gap_addr_t *scanned_addr) {
    for (uint32_t i = 0; i < product_count; i++) {
        if (scanned_addr->addr_type == product_list[i].addr.addr_type &&
            memcmp(scanned_addr->addr, product_list[i].addr.addr, BLE_GAP_ADDR_LEN) == 0) {
            return true;
        }
    }
    return false;
}

static void ble_evt_handler(ble_evt_t *p_ble_evt) {
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_ADV_REPORT: {
            const ble_gap_evt_adv_report_t *adv = &p_ble_evt->evt.gap_evt.params.adv_report;
            char addr_str[32];
            snprintf(addr_str, sizeof(addr_str), "Peer addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     adv->peer_addr.addr[5], adv->peer_addr.addr[4], adv->peer_addr.addr[3],
                     adv->peer_addr.addr[2], adv->peer_addr.addr[1], adv->peer_addr.addr[0]);
            SEGGER_RTT_WriteString(0, addr_str);

            if (is_address_in_list(&adv->peer_addr)) {
                char log[64];
                snprintf(log, sizeof(log), "Address %02X:%02X:%02X:%02X:%02X:%02X found in product list\n",
                         adv->peer_addr.addr[5], adv->peer_addr.addr[4], adv->peer_addr.addr[3],
                         adv->peer_addr.addr[2], adv->peer_addr.addr[1], adv->peer_addr.addr[0]);
                SEGGER_RTT_WriteString(0, log);
            } else {
                char log[64];
                snprintf(log, sizeof(log), "Address %02X:%02X:%02X:%02X:%02X:%02X not found in product list\n",
                         adv->peer_addr.addr[5], adv->peer_addr.addr[4], adv->peer_addr.addr[3],
                         adv->peer_addr.addr[2], adv->peer_addr.addr[1], adv->peer_addr.addr[0]);
                SEGGER_RTT_WriteString(0, log);
            }

            if (is_address_in_list(&adv->peer_addr) && is_uuid_present(&m_target_uuid, adv)) {
                SEGGER_RTT_WriteString(0, "NUS UUID found and address in product list, connecting...\n");
                memcpy(&connected_peer_addr, &adv->peer_addr, sizeof(ble_gap_addr_t));
                uint32_t err_code = sd_ble_gap_connect(&adv->peer_addr, &m_scan_params, &m_connection_param);
                if (err_code != NRF_SUCCESS) {
                    char log[32];
                    snprintf(log, sizeof(log), "Connection failed: %u\n", err_code);
                    SEGGER_RTT_WriteString(0, log);
                } else {
                    SEGGER_RTT_WriteString(0, "Connecting to peripheral...\n");
                }
            }
            break;
        }
        case BLE_GAP_EVT_CONNECTED: {
            const ble_gap_addr_t *peer = &p_ble_evt->evt.gap_evt.params.connected.peer_addr;
            char addr_str[32];
            snprintf(addr_str, sizeof(addr_str), "Connected to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     peer->addr[5], peer->addr[4], peer->addr[3],
                     peer->addr[2], peer->addr[1], peer->addr[0]);
            SEGGER_RTT_WriteString(0, addr_str);
            memcpy(&connected_peer_addr, peer, sizeof(ble_gap_addr_t));
            ble_db_discovery_start(&m_ble_db_discovery, p_ble_evt->evt.gap_evt.conn_handle);
            nrf_gpio_pin_set(LED_PIN);
            break;
        }
        case BLE_GAP_EVT_DISCONNECTED:
            SEGGER_RTT_WriteString(0, "Disconnected\n");
            if (product_count > 0) {
                scan_start();
            }
            break;
        case BLE_GAP_EVT_TIMEOUT:
            SEGGER_RTT_WriteString(0, "Connection timeout, retrying\n");
            if (product_count > 0) {
                scan_start();
            }
            break;
        default:
            break;
    }
}

static void ble_evt_dispatch(ble_evt_t *p_ble_evt) {
    ble_db_discovery_on_ble_evt(&m_ble_db_discovery, p_ble_evt);
    ble_nus_c_on_ble_evt(&m_ble_nus_c, p_ble_evt);
    ble_evt_handler(p_ble_evt);
}

static void log_central_address(void) {
    ble_gap_addr_t addr;
    uint32_t err_code = sd_ble_gap_address_get(&addr);
    if (err_code == NRF_SUCCESS) {
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "Central addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 addr.addr[5], addr.addr[4], addr.addr[3],
                 addr.addr[2], addr.addr[1], addr.addr[0]);
        SEGGER_RTT_WriteString(0, addr_str);
    } else {
        char log[32];
        snprintf(log, sizeof(log), "Get central address failed: %u\n", err_code);
        SEGGER_RTT_WriteString(0, log);
    }
}

static void ble_stack_init(void) {
    uint32_t err_code;
    nrf_clock_lf_cfg_t clock_lf_cfg = {
        .source = NRF_CLOCK_LF_SRC_XTAL,
        .rc_ctiv = 0,
        .rc_temp_ctiv = 0,
        .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM
    };

    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT, &ble_enable_params);
    APP_ERROR_CHECK(err_code);
    err_code = softdevice_enable(&ble_enable_params);
    if (err_code != NRF_SUCCESS) {
        char log[32];
        snprintf(log, sizeof(log), "SoftDevice enable failed: %u\n", err_code);
        SEGGER_RTT_WriteString(0, log);
    }
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    if (err_code != NRF_SUCCESS) {
        char log[32];
        snprintf(log, sizeof(log), "BLE evt handler set failed: %u\n", err_code);
        SEGGER_RTT_WriteString(0, log);
    }

    log_central_address();
}

static void timers_init(void) {
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
}

static bool parse_mac_address(const char *mac_str, ble_gap_addr_t *addr) {
    uint8_t mac_bytes[6];
    int ret = sscanf(mac_str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
                     &mac_bytes[5], &mac_bytes[4], &mac_bytes[3],
                     &mac_bytes[2], &mac_bytes[1], &mac_bytes[0]);
    if (ret != 6) {
        SEGGER_RTT_WriteString(0, "Invalid MAC address format\n");
        return false;
    }
    memcpy(addr->addr, mac_bytes, BLE_GAP_ADDR_LEN);
    addr->addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
    return true;
}

static void read_products_from_flash(void) {
    uint8_t *flash_ptr = (uint8_t *)FLASH_PAGE_ADDR;
    if (flash_ptr[0] != 0xFF) { // Check if flash is not erased
        product_count = 0;
        while (product_count < MAX_PRODUCTS && flash_ptr[product_count * sizeof(product_t)] != 0xFF) {
            memcpy(&product_list[product_count], &flash_ptr[product_count * sizeof(product_t)], sizeof(product_t));
            product_count++;
        }
        products_stored = (product_count > 0);
    }
}

static void write_products_to_flash(void) {
    flash_page_erase((uint32_t *)FLASH_PAGE_ADDR);
    
    uint32_t *flash_ptr = (uint32_t *)FLASH_PAGE_ADDR;
    uint8_t *data = (uint8_t *)product_list;
    for (uint32_t i = 0; i < (product_count * sizeof(product_t) + 3) / 4; i++) {
        uint32_t word = 0;
        memcpy(&word, &data[i * 4], sizeof(uint32_t));
        flash_word_write(flash_ptr + i, word);
    }
    products_stored = (product_count > 0);
}

static void erase_flash(void) {
    flash_page_erase((uint32_t *)FLASH_PAGE_ADDR);
    memset(product_list, 0, sizeof(product_list));
    product_count = 0;
    products_stored = false;
    SEGGER_RTT_WriteString(0, "Flash erased\n");
}

static void remove_product_from_list(uint32_t idx) {
    if (idx >= product_count) return;
    for (uint32_t i = idx; i < product_count - 1; i++) {
        product_list[i] = product_list[i + 1];
    }
    product_count--;
    memset(&product_list[product_count], 0, sizeof(product_t));
    write_products_to_flash();
}

static void uart_event_handler(app_uart_evt_t *p_event) {
    uint8_t c;
    static char command_buffer[32];
    static char log[128];
    
    switch (p_event->evt_type) {
        case APP_UART_DATA_READY:
            while (app_uart_get(&c) == NRF_SUCCESS) {
                if (c == '\r' || c == '\n') {
                    if (buffer_index > 0) {
                        uart_rx_buffer[buffer_index] = '\0';
                        if (uart_state == UART_STATE_COMMAND) {
                            if (strcmp((char *)uart_rx_buffer, "erase") == 0) {
                                erase_flash();
                                app_uart_flush();
                            }
                            uart_state = UART_STATE_MAC;
                        } else {
                            strncpy(command_buffer, (char *)uart_rx_buffer, sizeof(command_buffer) - 1);
                            command_buffer[sizeof(command_buffer) - 1] = '\0';
                            switch (uart_state) {
                                case UART_STATE_MAC:
                                    if (parse_mac_address(command_buffer, &temp_product.addr)) {
                                        uart_state = UART_STATE_ID;
                                        snprintf(log, sizeof(log), "Received MAC: %s\n", command_buffer);
                                        SEGGER_RTT_WriteString(0, log);
                                    } else {
                                        uart_state = UART_STATE_MAC;
                                    }
                                    break;
                                case UART_STATE_ID:
                                    strncpy(temp_product.id, command_buffer, sizeof(temp_product.id) - 1);
                                    temp_product.id[sizeof(temp_product.id) - 1] = '\0';
                                    uart_state = UART_STATE_NAME;
                                    snprintf(log, sizeof(log), "Received ID: %s\n", command_buffer);
                                    SEGGER_RTT_WriteString(0, log);
                                    break;
                                case UART_STATE_NAME:
                                    strncpy(temp_product.name, command_buffer, sizeof(temp_product.name) - 1);
                                    temp_product.name[sizeof(temp_product.name) - 1] = '\0';
                                    uart_state = UART_STATE_PRICE;
                                    snprintf(log, sizeof(log), "Received Name: %s\n", command_buffer);
                                    SEGGER_RTT_WriteString(0, log);
                                    break;
                                case UART_STATE_PRICE:
                                    strncpy(temp_product.price, command_buffer, sizeof(temp_product.price) - 1);
                                    temp_product.price[sizeof(temp_product.price) - 1] = '\0';
                                    uart_state = UART_STATE_DETAILS;
                                    snprintf(log, sizeof(log), "Received Price: %s\n", command_buffer);
                                    SEGGER_RTT_WriteString(0, log);
                                    break;
                                case UART_STATE_DETAILS:
                                    strncpy(temp_product.details, command_buffer, sizeof(temp_product.details) - 1);
                                    temp_product.details[sizeof(temp_product.details) - 1] = '\0';
                                    if (product_count < MAX_PRODUCTS) {
                                        memcpy(&product_list[product_count], &temp_product, sizeof(product_t));
                                        product_count++;
                                        write_products_to_flash();
                                        snprintf(log, sizeof(log), "Stored product #%u: MAC=%02X:%02X:%02X:%02X:%02X:%02X, ID=%s, Name=%s, Price=%s, Details=%s\n",
                                                 product_count,
                                                 temp_product.addr.addr[5], temp_product.addr.addr[4], temp_product.addr.addr[3],
                                                 temp_product.addr.addr[2], temp_product.addr.addr[1], temp_product.addr.addr[0],
                                                 temp_product.id, temp_product.name, temp_product.price, temp_product.details);
                                        SEGGER_RTT_WriteString(0, log);
                                    } else {
                                        SEGGER_RTT_WriteString(0, "Product list full\n");
                                    }
                                    uart_state = UART_STATE_MAC;
                                    break;
                            }
                        }
                        buffer_index = 0;
                    } else {
                        uart_state = UART_STATE_MAC;
                    }
                } else if (buffer_index < UART_BUFFER_SIZE - 1) {
                    uart_rx_buffer[buffer_index++] = c;
                    app_uart_put(c); // Echo back
                }
            }
            break;
        case APP_UART_COMMUNICATION_ERROR:
            SEGGER_RTT_WriteString(0, "UART communication error\n");
            break;
        default:
            break;
    }
}

static void nus_c_init(void) {
    ble_nus_c_init_t init = {0};
    init.evt_handler = ble_nus_c_evt_handler;
    uint32_t err_code = ble_nus_c_init(&m_ble_nus_c, &init);
    if (err_code != NRF_SUCCESS) {
        char log[32];
        snprintf(log, sizeof(log), "NUS client init failed: %u\n", err_code);
        SEGGER_RTT_WriteString(0, log);
    }
}

static void flash_page_erase(uint32_t *page_address) {
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_NVMC->ERASEPAGE = (uint32_t)page_address;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
}

static void flash_word_write(uint32_t *address, uint32_t value) {
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    *address = value;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
}

int main(void) {
    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "BLE Central Ready\n");

    uart_init();
    gpio_init();
    timers_init();
    ble_stack_init();
    ble_db_discovery_init(db_disc_handler);
    nus_c_init();

    ble_uuid128_t nus_base_uuid = BLE_NUS_BASE_UUID;
    uint32_t err_code = sd_ble_uuid_vs_add(&nus_base_uuid, &nus_uuid_type);
    if (err_code != NRF_SUCCESS) {
        char log[32];
        snprintf(log, sizeof(log), "UUID add failed: %u\n", err_code);
        SEGGER_RTT_WriteString(0, log);
    }
    m_target_uuid.type = nus_uuid_type;

    read_products_from_flash();
    if (products_stored && product_count > 0) {
        uart_receiving = false;
        scanning = true;
        SEGGER_RTT_WriteString(0, "Products found in flash, starting BLE scan\n");
        scan_start();
    } else {
        uart_receiving = true;
        SEGGER_RTT_WriteString(0, "No products in flash, waiting for UART data or switch toggle\n");
    }

    while (true) {
        sd_app_evt_wait();
    }
}

