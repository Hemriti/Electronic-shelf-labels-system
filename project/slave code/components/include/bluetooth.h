#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdint.h>
#include "ble.h"
#include "ble_gap.h"

// Constants
#define IS_SRVC_CHANGED_CHARACT_PRESENT 0
#define CENTRAL_LINK_COUNT              0
#define PERIPHERAL_LINK_COUNT           1
#define DEVICE_NAME                     "nrf_peripheral"
#define APP_ADV_INTERVAL                64
#define APP_ADV_TIMEOUT_IN_SECONDS      180
#define APP_TIMER_PRESCALER             0
#define APP_TIMER_OP_QUEUE_SIZE         4
#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(10000, UNIT_10_MS)
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)
#define MAX_CONN_PARAMS_UPDATE_COUNT    3

// Function declarations
void bluetooth_init(void);
void bluetooth_advertising_start(void);
void bluetooth_evt_dispatch(ble_evt_t *p_ble_evt);
void bluetooth_on_disconnect(void);

#endif // BLUETOOTH_H