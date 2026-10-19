/*
 * L-EVK-NRF54L15 application on the native Lierda board target.
 * LED0: P0.02, LED1: P0.03; UART30 TX: P0.01, RX: P0.00 (115200 8N1).
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "imu_lis2dw12.h"
#include "ppg_gh3220_hal.h"

#define BLINK_INTERVAL_MS 1000U
#define IMU_LOG_INTERVAL_MS 1000U

LOG_MODULE_REGISTER(l_evk_blinky, LOG_LEVEL_INF);

void l_evk_smp_bt_start(void);

static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_led0), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_led1), gpios),
};

int main(void)
{
	bool leds_on = false;
	int64_t next_imu_log_ms;
	int ret;

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
	(void)l_evk_ppg_hal_init();
	l_evk_smp_bt_start();
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
		//LOG_INF("LED P0.02/P0.03: %s", leds_on ? "ON" : "OFF");
		if (k_uptime_get() >= next_imu_log_ms) {
			l_evk_imu_log_sample();
			next_imu_log_ms += IMU_LOG_INTERVAL_MS;
		}
		k_msleep(BLINK_INTERVAL_MS);
	}
}
