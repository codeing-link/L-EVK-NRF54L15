/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "gh3220_ble_service.h"
#include "gh3220_protocol_user.h"

LOG_MODULE_REGISTER(gh3220_ble, LOG_LEVEL_INF);

#define GH3220_BLE_RX_QUEUE_DEPTH 4
#define GH3220_BLE_RX_PACKET_MAX  251
#define GH3220_BLE_TX_QUEUE_DEPTH 8
#define GH3220_BLE_TX_RETRY_COUNT 5

struct gh3220_ble_rx_packet {
	uint32_t session;
	uint16_t len;
	uint8_t data[GH3220_BLE_RX_PACKET_MAX];
};

struct gh3220_ble_tx_packet {
	uint32_t session;
	uint16_t len;
	uint8_t data[GH3220_BLE_RX_PACKET_MAX];
};

K_MSGQ_DEFINE(gh3220_ble_rx_msgq,
	      sizeof(struct gh3220_ble_rx_packet),
	      GH3220_BLE_RX_QUEUE_DEPTH, 4);
K_MSGQ_DEFINE(gh3220_ble_tx_msgq,
	      sizeof(struct gh3220_ble_tx_packet),
	      GH3220_BLE_TX_QUEUE_DEPTH, 4);

static struct k_spinlock connection_lock;
static struct bt_conn *current_conn;
static bool rx_notify_enabled;
static uint32_t connection_session;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
};
static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, GH3220_BLE_SERVICE_UUID),
	BT_DATA_BYTES(BT_DATA_NAME_SHORTENED, 'L', '-', 'E', 'V', 'K', '-', '5', '4', 'L'),
};

static int gh3220_ble_notify_now(const uint8_t *data, uint16_t len,
				uint32_t session);

static void gh3220_ble_tx_thread(void *p1, void *p2, void *p3)
{
	struct gh3220_ble_tx_packet packet;
	int ret = -EIO;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		if (k_msgq_get(&gh3220_ble_tx_msgq, &packet, K_FOREVER) != 0) {
			continue;
		}

		ret = gh3220_ble_notify_now(packet.data, packet.len,
					   packet.session);
		if (ret != 0 && ret != -ENOTCONN) {
			LOG_WRN("GH3220 BLE notify failed: %d", ret);
		}
	}
}

K_THREAD_DEFINE(gh3220_ble_tx_tid, 2048, gh3220_ble_tx_thread,
		NULL, NULL, NULL, 5, 0, 0);

static void gh3220_ble_rx_thread(void *p1, void *p2, void *p3)
{
	struct gh3220_ble_rx_packet packet;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		if (k_msgq_get(&gh3220_ble_rx_msgq, &packet, K_FOREVER) == 0) {
			gh3220_protocol_receive(packet.data, packet.len, packet.session);
		}
	}
}

K_THREAD_DEFINE(gh3220_ble_rx_tid, 4096, gh3220_ble_rx_thread,
		NULL, NULL, NULL, 5, 0, 0);

static int gh3220_ble_rx_data(const uint8_t *data, uint16_t len)
{
	struct gh3220_ble_rx_packet packet;

	if ((data == NULL) || (len == 0U)) {
		return -EINVAL;
	}
	if (len > sizeof(packet.data)) {
		return -EMSGSIZE;
	}

	packet.len = len;
	memcpy(packet.data, data, len);
	packet.session = gh3220_ble_session();
	int err = k_msgq_put(&gh3220_ble_rx_msgq, &packet, K_NO_WAIT);
	if (err) {
		LOG_WRN("GH3220 RX queue full, dropping %u bytes", len);
	}
	return err;
}

static ssize_t tx_write(struct bt_conn *conn,
			const struct bt_gatt_attr *attr,
			const void *buf, uint16_t len,
			uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);

	if (offset != 0U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
	}
	if (len == 0U || len > GH3220_BLE_RX_PACKET_MAX) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	if (gh3220_ble_rx_data(buf, len) != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
	return len;
}

static void rx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	bool enabled = value == BT_GATT_CCC_NOTIFY;
	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	rx_notify_enabled = enabled;
	if (!enabled) {
		connection_session++;
	}
	k_spin_unlock(&connection_lock, key);
	LOG_INF("GH3220 BLE RX notify %s", enabled ? "enabled" : "disabled");
	if (!enabled) {
		k_msgq_purge(&gh3220_ble_tx_msgq);
		gh3220_protocol_session_changed();
	}
}

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(GH3220_BLE_SERVICE_UUID);
static struct bt_uuid_128 rx_uuid = BT_UUID_INIT_128(GH3220_BLE_RX_UUID);
static struct bt_uuid_128 tx_uuid = BT_UUID_INIT_128(GH3220_BLE_TX_UUID);

BT_GATT_SERVICE_DEFINE(gh3220_service,
	BT_GATT_PRIMARY_SERVICE(&service_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&tx_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE, NULL, tx_write, NULL),
	BT_GATT_CHARACTERISTIC(&rx_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(rx_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static int gh3220_ble_notify_now(const uint8_t *data, uint16_t len,
				uint32_t session)
{
	struct bt_conn *conn = NULL;
	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	if (current_conn && rx_notify_enabled &&
	    session == connection_session) {
		conn = bt_conn_ref(current_conn);
	}
	k_spin_unlock(&connection_lock, key);
	if (conn == NULL) {
		return -ENOTCONN;
	}
	int err = 0;
	for (uint16_t offset = 0; offset < len;) {
		key = k_spin_lock(&connection_lock);
		bool valid = current_conn == conn && rx_notify_enabled &&
			     session == connection_session;
		k_spin_unlock(&connection_lock, key);
		if (!valid) {
			err = -ENOTCONN;
			break;
		}
		uint16_t mtu = bt_gatt_get_mtu(conn);
		uint16_t chunk = MIN(len - offset, mtu - 3U);
		for (int retry = 0; retry < GH3220_BLE_TX_RETRY_COUNT; retry++) {
			err = bt_gatt_notify(conn, &gh3220_service.attrs[4],
					     data + offset, chunk);
			if (err != -ENOMEM && err != -EAGAIN) {
				break;
			}
			k_sleep(K_MSEC(5));
		}
		if (err) {
			break;
		}
		offset += chunk;
	}
	bt_conn_unref(conn);
	return err;
}

static void adv_restart_handler(struct k_work *work);
K_WORK_DEFINE(adv_restart_work, adv_restart_handler);

static void adv_restart_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	int err = gh3220_ble_advertise();
	if (err) {
		LOG_ERR("Restart advertising failed: %d", err);
	} else {
		LOG_INF("Advertising restarted");
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0U) {
		LOG_ERR("GH3220 BLE connection failed: %u", err);
		return;
	}

	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	current_conn = bt_conn_ref(conn);
	connection_session++;
	k_spin_unlock(&connection_lock, key);
	gh3220_protocol_session_changed();
	LOG_INF("GH3220 BLE connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("GH3220 BLE disconnected: reason=%u", reason);
	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	struct bt_conn *old_conn = current_conn;
	current_conn = NULL;
	rx_notify_enabled = false;
	connection_session++;
	k_spin_unlock(&connection_lock, key);
	if (old_conn != NULL) {
		bt_conn_unref(old_conn);
	}
	k_msgq_purge(&gh3220_ble_rx_msgq);
	k_msgq_purge(&gh3220_ble_tx_msgq);
	gh3220_protocol_session_changed();

}

static void recycled(void)
{
	/* The controller can advertise again once the connection slot is free. */
	k_work_submit(&adv_restart_work);
}

BT_CONN_CB_DEFINE(gh3220_conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

int gh3220_ble_advertise(void)
{
	/* The static work item is ready before any connection callback fires. */
	return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
}

int l_evk_bt_start(void)
{
	int err = bt_enable(NULL);

	if (err != 0) {
		return err;
	}

	err = gh3220_ble_advertise();
	if (err == 0) {
		LOG_INF("BLE advertising started with SMP and GH3220 services");
	}
	return err;
}

int gh3220_ble_notify(const uint8_t *data, uint16_t len)
{
	struct gh3220_ble_tx_packet packet;

	if ((data == NULL) || (len == 0U)) {
		return -EINVAL;
	}
	if (len > sizeof(packet.data)) {
		return -EMSGSIZE;
	}

	packet.len = len;
	memcpy(packet.data, data, len);
	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	packet.session = connection_session;
	bool ready = current_conn != NULL && rx_notify_enabled;
	k_spin_unlock(&connection_lock, key);
	return ready ? k_msgq_put(&gh3220_ble_tx_msgq, &packet, K_MSEC(100)) : -ENOTCONN;
}

bool gh3220_ble_is_connected(void)
{
	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	bool connected = current_conn != NULL;
	k_spin_unlock(&connection_lock, key);
	return connected;
}

uint32_t gh3220_ble_session(void)
{
	k_spinlock_key_t key = k_spin_lock(&connection_lock);
	uint32_t session = connection_session;
	k_spin_unlock(&connection_lock, key);
	return session;
}
