/*
 * L-EVK-NRF54L15 application on the native Lierda board target.
 * LED0: P0.02, LED1: P0.03; UART30 TX: P0.01, RX: P0.00 (115200 8N1).
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include "app_thread.h"
#include "gh3220_ble_service.h"
#include "gh3220_framework.h"
#include "gh3220_scheduler.h"
#include "imu_lis2dw12.h"

#define BLINK_INTERVAL_MS 1000U
#define IMU_LOG_INTERVAL_MS 1000U

/*
 * Application log switch:
 *   LOG_LEVEL_INF (default): normal startup/status logs; hide periodic data.
 *   LOG_LEVEL_DBG:           also print LED state and IMU acceleration.
 *   LOG_LEVEL_WRN/ERR/NONE:  progressively quieter output.
 */
#define L_EVK_LOG_LEVEL LOG_LEVEL_INF

BUILD_ASSERT(L_EVK_LOG_LEVEL >= LOG_LEVEL_NONE &&
	     L_EVK_LOG_LEVEL <= LOG_LEVEL_DBG,
	     "L_EVK_LOG_LEVEL must be a Zephyr LOG_LEVEL_* value");

/* Compile DBG messages in; L_EVK_LOG_LEVEL selects the runtime filter. */
LOG_MODULE_REGISTER(l_evk_blinky, LOG_LEVEL_DBG);

static void l_evk_set_log_level(void)
{
	static const char *const sources[] = {
		"l_evk_blinky",
		"l_evk_imu",
		"gh3220_port",
		"gh3220_raw_data",
		"gh3220_ble",
	};

	for (size_t i = 0U; i < ARRAY_SIZE(sources); ++i) {
		int source_id = log_source_id_get(sources[i]);

		if (source_id >= 0) {
			(void)log_filter_set(NULL, 0U, source_id, L_EVK_LOG_LEVEL);
		}
	}
}

static void l_evk_log_reset_cause(void)
{
	uint32_t cause;
	int ret = hwinfo_get_reset_cause(&cause);

	if (ret != 0) {
		LOG_WRN("Cannot read reset cause: %d", ret);
		return;
	}

	LOG_INF("Reset cause: raw=0x%08x", cause);
	if (cause & RESET_PIN) {
		LOG_INF("Reset cause flag: external pin");
	}
	if (cause & RESET_SOFTWARE) {
		LOG_INF("Reset cause flag: software");
	}
	if (cause & RESET_BROWNOUT) {
		LOG_INF("Reset cause flag: brownout");
	}
	if (cause & RESET_POR) {
		LOG_INF("Reset cause flag: power-on");
	}
	if (cause & RESET_WATCHDOG) {
		LOG_INF("Reset cause flag: watchdog");
	}
	if (cause & RESET_DEBUG) {
		LOG_INF("Reset cause flag: debug");
	}
	if (cause & RESET_LOW_POWER_WAKE) {
		LOG_INF("Reset cause flag: low-power wake");
	}
	if (cause & RESET_CPU_LOCKUP) {
		LOG_INF("Reset cause flag: CPU lockup");
	}
	if (cause & RESET_HARDWARE) {
		LOG_INF("Reset cause flag: hardware");
	}
	if (cause & RESET_BOOTLOADER) {
		LOG_INF("Reset cause flag: bootloader");
	}

	ret = hwinfo_clear_reset_cause();
	if (ret != 0) {
		LOG_WRN("Cannot clear reset cause: %d", ret);
	}
}

static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_led0), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_led1), gpios),
};

int main(void)
{
	bool leds_on = false;
	int64_t next_imu_log_ms;
	int ret;

	l_evk_set_log_level();
	l_evk_log_reset_cause();

	for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
		if (!gpio_is_ready_dt(&leds[i])) {
			LOG_ERR("LED%u GPIO controller is not ready", (unsigned int)i);
			return 0;
		}

		ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (ret != 0) {
			LOG_ERR("Cannot configure LED%u: %d", (unsigned int)i, ret);
			return 0;
		}
	}

	LOG_INF("L-EVK-NRF54L15 started");
	LOG_INF("UART30: TX=P0.01 RX=P0.00, 115200 8N1");
	LOG_INF("LED0=P0.02 LED1=P0.03; blink interval=%u ms", BLINK_INTERVAL_MS);
	(void)l_evk_imu_init();

	app_thread_set_tick_period_ms(1000U);
	app_thread_init();
	ret = gh3220_framework_wait_ready();
	if (ret != 0) {
		LOG_ERR("GH3220 SDK initialization failed: %d", ret);
	} else {
		gh3220_scheduler_start();
		LOG_INF("GH3220 health framework started");
	}

	ret = l_evk_bt_start();
	if (ret != 0) {
		LOG_ERR("Bluetooth initialization failed: %d", ret);
	}
	next_imu_log_ms = k_uptime_get();

	while (true) {
		for (size_t i = 0; i < ARRAY_SIZE(leds); ++i) {
			ret = gpio_pin_toggle_dt(&leds[i]);
			if (ret != 0) {
				LOG_ERR("Cannot toggle LED%u: %d", (unsigned int)i, ret);
				return 0;
			}
		}

		leds_on = !leds_on;
		LOG_DBG("LED P0.02/P0.03: %s", leds_on ? "ON" : "OFF");
		if (k_uptime_get() >= next_imu_log_ms) {
			l_evk_imu_log_sample();
			next_imu_log_ms += IMU_LOG_INTERVAL_MS;
		}
		k_msleep(BLINK_INTERVAL_MS);
	}
}
