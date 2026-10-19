/* Bluetooth LE advertising for the Zephyr MCUmgr SMP OTA service. */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>

LOG_MODULE_REGISTER(l_evk_smp_bt, LOG_LEVEL_INF);

static struct k_work advertise_work;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err != 0) {
		LOG_ERR("Cannot start OTA advertising: %d", err);
		return;
	}

	LOG_INF("BLE OTA advertising started: %s", CONFIG_BT_DEVICE_NAME);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	ARG_UNUSED(conn);

	if (err != 0) {
		LOG_WRN("BLE connection failed: 0x%02x (%s)", err, bt_hci_err_to_str(err));
		k_work_submit(&advertise_work);
		return;
	}

	LOG_INF("BLE OTA client connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("BLE OTA client disconnected: 0x%02x (%s)", reason, bt_hci_err_to_str(reason));
}

static void recycled(void)
{
	k_work_submit(&advertise_work);
}

BT_CONN_CB_DEFINE(l_evk_conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

static void bt_ready(int err)
{
	if (err != 0) {
		LOG_ERR("Bluetooth initialization failed: %d", err);
		return;
	}

	k_work_submit(&advertise_work);
}

void l_evk_smp_bt_start(void)
{
	int err;

	k_work_init(&advertise_work, advertise);
	err = bt_enable(bt_ready);
	if (err != 0) {
		LOG_ERR("Cannot enable Bluetooth: %d", err);
	}
}
