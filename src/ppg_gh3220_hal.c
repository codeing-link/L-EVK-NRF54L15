/*
 * GH3220 hardware-abstraction layer.
 *
 * This module deliberately initializes only the SPI transport, reset output,
 * and interrupt input. GH3220 register access and PPG algorithms are not
 * implemented yet.
 */

#include "ppg_gh3220_hal.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(l_evk_ppg_hal, LOG_LEVEL_INF);

static const struct device *const gh3220_spi = DEVICE_DT_GET(DT_NODELABEL(spi21));
static const struct gpio_dt_spec gh3220_cs =
	GPIO_DT_SPEC_GET(DT_NODELABEL(spi21), cs_gpios);
static const struct gpio_dt_spec gh3220_reset =
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_ppg_reset), gpios);
static const struct gpio_dt_spec gh3220_int =
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_ppg_int), gpios);

int l_evk_ppg_hal_init(void)
{
	int ret;

	if (!device_is_ready(gh3220_spi)) {
		LOG_ERR("GH3220 SPI21 is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&gh3220_cs) || !gpio_is_ready_dt(&gh3220_reset) ||
	    !gpio_is_ready_dt(&gh3220_int)) {
		LOG_ERR("GH3220 GPIO controller is not ready");
		return -ENODEV;
	}

	/* CS is active-low: leave it deasserted while no GH3220 transactions run. */
	ret = gpio_pin_configure_dt(&gh3220_cs, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Cannot configure PPG_SPI_CS (P1.11): %d", ret);
		return ret;
	}

	/* RST is active-low. Keep GH3220 out of reset after power-up. */
	ret = gpio_pin_configure_dt(&gh3220_reset, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Cannot configure PPG_RST (P1.09): %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&gh3220_int, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Cannot configure PPG_INT (P1.14): %d", ret);
		return ret;
	}

	k_msleep(2);
	LOG_INF("GH3220 HAL ready: SPI21 SCK=P1.12 MOSI=P1.13 MISO=P1.10 CS=P1.11");
	LOG_INF("GH3220 register driver is intentionally not enabled");
	return 0;
}
