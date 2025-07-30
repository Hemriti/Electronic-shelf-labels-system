/**
 * BLE Central code for nRF51822 (SDK 12.3.0) with RTT debug
 * Connects to a device advertising Nordic UART Service and uses RTT to log state.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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
#include "uart.h"
#include "bluetooth.h"
/*ble central parameters*/
#define CENTRAL_LINK_COUNT      1 //# of peripherals this central can connect to,
#define PERIPHERAL_LINK_COUNT   0 // This device does not advertise, if it take 1 , so will be peripheral and central in same time
/*Timer parameters*/
#define APP_TIMER_PRESCALER     0 // no devision,prescaler is null so the timer ticks at the base frequency
#define APP_TIMER_OP_QUEUE_SIZE 2//safety buffer for how many start/stop/create commands can be waiting
/*MTU size*/
#define NRF_BLE_MAX_MTU_SIZE    30 //The max GATT MTU size our app supports 30 bytes
/*Scanning parameters*/
#define SCAN_INTERVAL           0x00A0// 00A0 is 160 in decimal ,scan every 100ms= 160*0.625ms(from ble specification)
#define SCAN_WINDOW             0x0050 //Duration of each scan 0050is 80 in decimal ,so each scan take 0.625ms*80=50ms
#define SCAN_TIMEOUT            0x0064  // ~4 seconds,It defines how long the BLE Central device will keep scanning before stopping automatically
/*central-peripheral communication synchronisation, its set between 20 an d 75 ms*/
#define MIN_CONNECTION_INTERVAL MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONNECTION_INTERVAL MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define SLAVE_LATENCY           0//the peripheral must repond to every connection event without skipping any(latency 0)
#define SUPERVISION_TIMEOUT     MSEC_TO_UNITS(4000, UNIT_10_MS)//If I (the Central) don’t hear anything for 4 seconds, I’ll assume the connection is dead.
#define MSG_INTERVAL            APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER) // 2s 

#define LED_PIN 21

static ble_db_discovery_t m_ble_db_discovery;//ble_db_discovery a struct provided by the nordic and it contains internal state and buffers used during discovery process,and it will manages (services,srv_count,curr_char_ind...) and we usually pass it to functions like (ble_db_discovery_start,)
static ble_nus_c_t m_ble_nus_c;//a struct that hold all the necessary information about NUS and  it stores the connection handle, state of the client, any event handlers, and buffers related to receiving and sending data through the Nordic UART Service
APP_TIMER_DEF(m_msg_timer_id);//a macro to create an instance ofa a timer 
static bool nus_ready = false;
static uint8_t nus_uuid_type;
/*definition of the 128-bit UUID */
static const ble_uuid128_t BLE_NUS_BASE_UUID = {
    {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
     0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E}
};
/*structure used to configure the BLE scanning process on a Nordic device*/
static ble_gap_scan_params_t m_scan_params = {
    .active   = 1,//active scanning, the device will send a scan request to nearby devices,if it take 0 it will be passive scan so just listen fot advertising without requesting extra infos
    .interval = SCAN_INTERVAL,
    .window   = SCAN_WINDOW,
    .timeout  = SCAN_TIMEOUT,

	#if (NRF_SD_BLE_API_VERSION == 2)
    .selective   = 0,
    .p_whitelist = NULL,
#endif
#if (NRF_SD_BLE_API_VERSION == 3)
    .use_whitelist = 0,
#endif
};
/*definning the gap connection parameters*/
static const ble_gap_conn_params_t m_connection_param = {
    (uint16_t)MIN_CONNECTION_INTERVAL,
    (uint16_t)MAX_CONNECTION_INTERVAL,
    (uint16_t)SLAVE_LATENCY,
    (uint16_t)SUPERVISION_TIMEOUT
};

static ble_uuid_t m_target_uuid = {//This structure is a compact way to reference the NUS UUID in GAP and GATT operations.
    .uuid = BLE_UUID_NUS_SERVICE,//0x0001, for nus service
    .type = 0 // to be set after UUID registration
};

static void gpio_init(void) {
    NRF_GPIO->DIRSET = (1 << LED_PIN);
    NRF_GPIO->OUTCLR = (1 << LED_PIN);
}

static void send_hello(void * p_context) {
    if (!nus_ready) return;
    static const char msg[] = "hello !\n";
    uint32_t err_code = ble_nus_c_string_send(&m_ble_nus_c, (uint8_t *)msg, strlen(msg));
    if (err_code == NRF_SUCCESS) {
        SEGGER_RTT_WriteString(0, "[TX] hello from nrf!\n");
    }
}
/* function to start the ble scanning process*/
static void scan_start(void) {
    uint32_t err_code = sd_ble_gap_scan_start(&m_scan_params);//starting ble scan based on the m_scan _param
    APP_ERROR_CHECK(err_code);
    SEGGER_RTT_WriteString(0, "Scanning started...\n");
}
/*function used to check a specific 128-bit uuid is present in the adv report received from a nearby ble device(it filters based on uuid)*/
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
/*callback function triggered by the BLE stack when certain NUS-related events occur (like connection, receiving data, disconnection*/
static void ble_nus_c_evt_handler(ble_nus_c_t * p_ble_nus_c, const ble_nus_c_evt_t * p_ble_nus_evt) {
    switch (p_ble_nus_evt->evt_type) {
        case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
            ble_nus_c_handles_assign(p_ble_nus_c, p_ble_nus_evt->conn_handle, &p_ble_nus_evt->handles);
            ble_nus_c_rx_notif_enable(p_ble_nus_c);
            nus_ready = true;
            app_timer_start(m_msg_timer_id, MSG_INTERVAL, NULL);
            SEGGER_RTT_WriteString(0, "? NUS ready, sending messages\n");
            break;

        case BLE_NUS_C_EVT_NUS_RX_EVT:
            SEGGER_RTT_WriteString(0, "[RX] ");
            SEGGER_RTT_Write(0, p_ble_nus_evt->p_data, p_ble_nus_evt->data_len);
            SEGGER_RTT_WriteString(0, "\n");

            // Turn on LED if we receive '1'
            if (p_ble_nus_evt->data_len == 1) {
    if (p_ble_nus_evt->p_data[0] == '1') {
        NRF_GPIO->OUTSET = (1 << LED_PIN);
        SEGGER_RTT_WriteString(0, "LED ON\n");
    } else if (p_ble_nus_evt->p_data[0] == '0') {
        NRF_GPIO->OUTCLR = (1 << LED_PIN);
        SEGGER_RTT_WriteString(0, "LED OFF\n");
    }
}
            break;

        case BLE_NUS_C_EVT_DISCONNECTED:
            SEGGER_RTT_WriteString(0, "? Disconnected\n");
            nus_ready = false;
            scan_start();
            break;

        default:
            break;
    }
}


static void db_disc_handler(ble_db_discovery_evt_t * p_evt) {
    ble_nus_c_on_db_disc_evt(&m_ble_nus_c, p_evt);
}
/*function is at the core of BLE event management It's the device's reaction system to any Bluetooth event coming from the SoftDevice */
static void ble_evt_handler(ble_evt_t * p_ble_evt) {
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_ADV_REPORT: {//This event is triggered when the Central device receives an advertisement packet from a Peripheral during the scanning process.The advertisement data is stored in ble_gap_evt_adv_report_t
            const ble_gap_evt_adv_report_t * adv = &p_ble_evt->evt.gap_evt.params.adv_report;
            SEGGER_RTT_WriteString(0, "?? ADV detected\n");
            if (is_uuid_present(&m_target_uuid, adv)) {//check uuid
                sd_ble_gap_connect(&adv->peer_addr, &m_scan_params, &m_connection_param);//connect
            }
            break;
        }
        case BLE_GAP_EVT_CONNECTED://Logs connection, starts discovery.
            SEGGER_RTT_WriteString(0, "Connected\n");
            ble_db_discovery_start(&m_ble_db_discovery, p_ble_evt->evt.gap_evt.conn_handle);
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            SEGGER_RTT_WriteString(0, "Reconnecting\n");
            scan_start();
            break;
        default:
            break;
    }
}

static void ble_evt_dispatch(ble_evt_t * p_ble_evt) {
    ble_nus_c_on_ble_evt(&m_ble_nus_c, p_ble_evt);
    ble_db_discovery_on_ble_evt(&m_ble_db_discovery, p_ble_evt);
    ble_evt_handler(p_ble_evt);
}
/*Initialize the Nordic SoftDevice (BLE stack) to manage the BLE radio, protocol, and events, configuring the device as a Central with one connection.*/
static void ble_stack_init(void) {
    uint32_t err_code;//Declares a variable to store error codes returned by SoftDevice API calls.to indicate success or failure. This variable is used to check for errors, though not explicitly checked here 
    nrf_clock_lf_cfg_t clock_lf_cfg = {//Purpose: Configures the low-frequency (32 kHz) clock, required by the SoftDevice for accurate timing of BLE events (e.g., scanning, connection intervals).
        .source = NRF_CLOCK_LF_SRC_RC,//Uses the internal RC oscillator, a low-power option suitable for the nRF51822
        .rc_ctiv = 16,// Sets calibration interval to 16 * 0.25 seconds = 4 seconds, balancing accuracy and power consumption.
        .rc_temp_ctiv = 2//Calibrates every 2 unit changes to adjust for drift.
    };
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);//Initializes the SoftDevice’s system-level components, including the clock and scheduler.

    ble_enable_params_t ble_enable_params;// Declares a structure to hold enable configuration parameters for enabling the BLE stack.
    softdevice_enable_get_default_config(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT, &ble_enable_params);// Fills ble_enable_params with default settings for the specified number of links. The SoftDevice needs to know the device’s role and connection capacity to allocate RAM and configure the stack. Setting PERIPHERAL_LINK_COUNT = 0 ensures the device operates only as a Central.
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);//Verifies that the application’s RAM start address aligns with the SoftDevice’s memory requirements.
    softdevice_enable(&ble_enable_params);// Activates the SoftDevice with the configured parameters.Enables the BLE stack, allocating memory for one Central link and initializing the radio and protocol layers.
    softdevice_ble_evt_handler_set(ble_evt_dispatch);// Registers the ble_evt_dispatch function to handle BLE events (e.g., advertisements, connections).
}

static void timers_init(void) {
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, NULL);
    app_timer_create(&m_msg_timer_id, APP_TIMER_MODE_REPEATED, send_hello);
}

static void nus_c_init(void) {
    ble_nus_c_init_t init = {.evt_handler = ble_nus_c_evt_handler};
    ble_nus_c_init(&m_ble_nus_c, &init);
}

int main(void) {
    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "BLE Central Ready\n");

    gpio_init();
    timers_init();
    ble_stack_init();
    ble_db_discovery_init(db_disc_handler);
    nus_c_init();
/*Register the UUID*/
    ble_uuid128_t nus_base_uuid = BLE_NUS_BASE_UUID; // Copy NUS_BASE_UUID to nus_base_uuid for the SoftDevice API
sd_ble_uuid_vs_add(&nus_base_uuid, &nus_uuid_type); // Add the 16-byte UUID to the SoftDevice’s vendor-specific UUID table, which assigns a type
m_target_uuid.type = nus_uuid_type; // Update m_target_uuid with the assigned type for efficient BLE operations
    scan_start();

    while (true) {
        sd_app_evt_wait();
    }
}
