//#include <stdbool.h>
//#include <stdint.h>
//#include <string.h>
//#include <stdio.h>
//#include "nordic_common.h"
//#include "nrf.h"
//#include "app_error.h"
//#include "ble.h"
//#include "ble_gap.h"
//#include "ble_hci.h"
//#include "softdevice_handler.h"
//#include "SEGGER_RTT.h"
//#include "ble_db_discovery.h"
//#include "ble_nus_c.h"
//#include "app_timer.h"
//#include "nrf_sdm.h"
//#include "nrf_clock.h"
//#include "nus.h"

///* BLE Central parameters */
//#define CENTRAL_LINK_COUNT      1
//#define PERIPHERAL_LINK_COUNT   0
//#define APP_TIMER_PRESCALER     0
//#define APP_TIMER_OP_QUEUE_SIZE 2
//#define NRF_BLE_MAX_MTU_SIZE    23 // S130 supports only 23 bytes for ATT MTU
//#define SCAN_INTERVAL           0x00A0 // 160 ms
//#define SCAN_WINDOW             0x0050 // 80 ms
//#define SCAN_TIMEOUT            0x0064 // 10 seconds
//#define MIN_CONNECTION_INTERVAL MSEC_TO_UNITS(20, UNIT_1_25_MS)
//#define MAX_CONNECTION_INTERVAL MSEC_TO_UNITS(75, UNIT_1_25_MS)
//#define SLAVE_LATENCY           0
//#define SUPERVISION_TIMEOUT     MSEC_TO_UNITS(4000, UNIT_10_MS)
//#define MSG_INTERVAL            APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER)

///* Store the connected peripheral's address */
//static ble_gap_addr_t connected_peer_addr;


//static ble_db_discovery_t m_ble_db_discovery;







//static void send_nus_string(const char *str) ;



//static void scan_start(void) ;

//static bool is_uuid_present(const ble_uuid_t *p_target_uuid, const ble_gap_evt_adv_report_t *p_adv_report) ;

//static void db_disc_handler(ble_db_discovery_evt_t *p_evt) ;

//static void ble_evt_handler(ble_evt_t *p_ble_evt);

//static void ble_evt_dispatch(ble_evt_t *p_ble_evt) ;

//static void log_central_address(void) ;
//static void ble_stack_init(void) ;


