#ifndef NUS_H
#define NUS_H

#include <stdbool.h>
#include <stdint.h>
#include "ble.h"
#include "ble_nus_c.h"
#include "ble_gap.h"

#define NRF_BLE_MAX_MTU_SIZE 23

extern const ble_uuid128_t BLE_NUS_BASE_UUID;

void nus_c_init(void);
void set_nus_uuid_type(uint8_t type);
void set_nus_connected_peer_addr(const ble_gap_addr_t *addr);
void db_disc_handler(ble_db_discovery_evt_t *p_evt);

#endif