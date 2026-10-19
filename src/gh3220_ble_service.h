/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/uuid.h>

/*
 * GH3220 BLE transport used by ComBridge.
 *
 * GATT UUIDs from the reference application carrying GH3220 uprotocol frames.
 */
/* ghealth-tools default service UUID: 0x190e. */
#define GH3220_BLE_SERVICE_UUID \
	BT_UUID_128_ENCODE(0x0000190e, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
/* Slave sends responses/data through RX (0x0003). */
#define GH3220_BLE_RX_UUID \
	BT_UUID_128_ENCODE(0x00000003, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
/* Master writes commands to TX (0x0004). */
#define GH3220_BLE_TX_UUID \
	BT_UUID_128_ENCODE(0x00000004, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)

int l_evk_bt_start(void);
int gh3220_ble_advertise(void);
int gh3220_ble_notify(const uint8_t *data, uint16_t len);
bool gh3220_ble_is_connected(void);
uint32_t gh3220_ble_session(void);
