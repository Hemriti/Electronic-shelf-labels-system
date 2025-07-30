#ifndef CUSTOM_NUS_H
#define CUSTOM_NUS_H

#include <stdint.h>
#include "ble_nus.h"
#include "ble_gap.h"

// Constants
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN
#define PACKET_QUEUE_SIZE               4
#define MAX_PACKET_LEN                  BLE_NUS_MAX_DATA_LEN

// Packet structure
typedef struct {
    uint8_t data[MAX_PACKET_LEN];
    uint16_t length;
} packet_t;

// Receive state enumeration
typedef enum {
    RECEIVE_STATE_IDLE,
    RECEIVE_STATE_ID,
    RECEIVE_STATE_NAME,
    RECEIVE_STATE_PRICE,
    RECEIVE_STATE_DETAILS
} receive_state_t;

// Function declarations
void custom_nus_services_init(void);
void custom_nus_on_ble_evt(ble_evt_t *p_ble_evt);
ble_uuid_t *custom_nus_get_adv_uuids(void);
void custom_nus_send_string(const char *str);
bool custom_nus_enqueue_packet(uint8_t *p_data, uint16_t length);
bool custom_nus_dequeue_packet(uint8_t *p_data, uint16_t *p_length);
void custom_nus_process_packet(uint8_t *p_data, uint16_t length);
void uint32_to_hex(uint32_t value, char *buf, uint8_t digits);
void strcat_safe(char *dest, const char *src, uint32_t max_len);

#endif // CUSTOM_NUS_H