/*
 * LIS2DW12TR hardware-I2C polling interface for L-EVK.
 */

#include "imu_lis2dw12.h"

#include <errno.h>
#include <stdint.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <hal/nrf_nfct.h>

#define LIS2DW12_WHO_AM_I_REG      0x0FU
#define LIS2DW12_WHO_AM_I_VALUE    0x44U
#define LIS2DW12_CTRL1_REG         0x20U
#define LIS2DW12_CTRL3_REG         0x22U
#define LIS2DW12_OUT_X_L_REG       0x28U

#define LIS2DW12_CTRL1_25HZ_HP     0x34U
#define LIS2DW12_CTRL3_BDU         0x08U
#define LIS2DW12_GAIN_UG_PER_LSB   244LL
#define SENSOR_G_UM_S2             9806650LL

LOG_MODULE_REGISTER(l_evk_imu, LOG_LEVEL_INF);

static const struct i2c_dt_spec imu_i2c =
	I2C_DT_SPEC_GET(DT_NODELABEL(lis2dw12));
static const struct gpio_dt_spec imu_int =
	GPIO_DT_SPEC_GET(DT_NODELABEL(l_evk_imu_int), gpios);

static bool imu_ready;

static void imu_release_nfc_pads(void)
{
	/*
	 * P1.03 doubles as NFC2 and is assigned to NFCT after reset. Disabling
	 * PADCONFIG releases the physical pad for the TWIM20 SCL signal.
	 */
#if NRF_NFCT_HAS_PAD_CONFIG_REG
	nrf_nfct_pad_config_enable_set(NRF_NFCT, false);
#endif
}

static int lis2dw12_write_reg(uint8_t reg, uint8_t value)
{
	const uint8_t buffer[] = { reg, value };

	return i2c_write_dt(&imu_i2c, buffer, sizeof(buffer));
}

static int lis2dw12_read_regs(uint8_t reg, uint8_t *data, size_t length)
{
	return i2c_write_read_dt(&imu_i2c, &reg, sizeof(reg), data, length);
}

static uint64_t absolute_value(int64_t value)
{
	return value < 0 ? (uint64_t)(-value) : (uint64_t)value;
}

static int64_t raw_to_um_s2(int16_t raw)
{
	/* High-performance output is left aligned; the official driver shifts 2. */
	return ((int64_t)(raw >> 2) * LIS2DW12_GAIN_UG_PER_LSB * SENSOR_G_UM_S2) /
	       1000000LL;
}

int l_evk_imu_init(void)
{
	uint8_t value = 0U;
	int ret;

	imu_release_nfc_pads();
	if (!i2c_is_ready_dt(&imu_i2c)) {
		LOG_ERR("IMU I2C20 controller is not ready");
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&imu_int)) {
		LOG_ERR("IMU interrupt GPIO controller is not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&imu_int, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Cannot configure ACC_INT (P1.04): %d", ret);
		return ret;
	}
	ret = lis2dw12_read_regs(LIS2DW12_WHO_AM_I_REG, &value, 1U);
	if (ret != 0 || value != LIS2DW12_WHO_AM_I_VALUE) {
		LOG_ERR("LIS2DW12TR hardware-I2C probe failed: ret=%d id=0x%02x", ret, value);
		return ret != 0 ? ret : -ENODEV;
	}
	ret = lis2dw12_read_regs(LIS2DW12_CTRL3_REG, &value, 1U);
	if (ret == 0) {
		ret = lis2dw12_write_reg(LIS2DW12_CTRL3_REG, value | LIS2DW12_CTRL3_BDU);
	}
	if (ret == 0) {
		ret = lis2dw12_write_reg(LIS2DW12_CTRL1_REG, LIS2DW12_CTRL1_25HZ_HP);
	}
	if (ret != 0) {
		LOG_ERR("LIS2DW12TR hardware-I2C configuration failed: %d", ret);
		return ret;
	}

	imu_ready = true;
	LOG_INF("LIS2DW12TR ready: TWIM20 SCL=P1.03 SDA=P1.06, 100 kHz, ODR=25 Hz, +/-2 g");
	return 0;
}

void l_evk_imu_log_sample(void)
{
	uint8_t buffer[6];
	int16_t raw[3];
	int64_t x;
	int64_t y;
	int64_t z;
	int ret;

	if (!imu_ready) {
		if (l_evk_imu_init() != 0) {
			return;
		}
	}
	ret = lis2dw12_read_regs(LIS2DW12_OUT_X_L_REG, buffer, sizeof(buffer));
	if (ret != 0) {
		LOG_ERR("LIS2DW12TR hardware-I2C sample read failed: %d", ret);
		imu_ready = false;
		return;
	}
	for (size_t i = 0U; i < ARRAY_SIZE(raw); ++i) {
		raw[i] = (int16_t)((uint16_t)buffer[i * 2U] |
					 ((uint16_t)buffer[i * 2U + 1U] << 8));
	}

	x = raw_to_um_s2(raw[0]);
	y = raw_to_um_s2(raw[1]);
	z = raw_to_um_s2(raw[2]);
	LOG_INF("ACC[m/s2] X=%c%llu.%03llu Y=%c%llu.%03llu Z=%c%llu.%03llu",
		x < 0 ? '-' : '+', (unsigned long long)(absolute_value(x) / 1000000U),
		(unsigned long long)((absolute_value(x) % 1000000U) / 1000U),
		y < 0 ? '-' : '+', (unsigned long long)(absolute_value(y) / 1000000U),
		(unsigned long long)((absolute_value(y) % 1000000U) / 1000U),
		z < 0 ? '-' : '+', (unsigned long long)(absolute_value(z) / 1000000U),
		(unsigned long long)((absolute_value(z) % 1000000U) / 1000U));
}
