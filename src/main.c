/* L-EVK-NRF54L15: LEDs P0.02/P0.03 and UART30 TX=P0.00 RX=P0.01. */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define BLINK_PERIOD_MS 1000
#define GPIO0_NODE DT_NODELABEL(gpio0)

#if !DT_NODE_HAS_STATUS_OKAY(GPIO0_NODE)
#error "The L-EVK-NRF54L15 board must enable gpio0"
#endif

static const struct gpio_dt_spec leds[] = {
	{
		.port = DEVICE_DT_GET(GPIO0_NODE),
		.pin = 2,
		.dt_flags = GPIO_ACTIVE_HIGH,
	},
	{
		.port = DEVICE_DT_GET(GPIO0_NODE),
		.pin = 3,
		.dt_flags = GPIO_ACTIVE_HIGH,
	},
};

int main(void)
{
	bool leds_on = false;
	int ret;

	if (!gpio_is_ready_dt(&leds[0])) {
		return 0;
	}

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (ret != 0) {
			return 0;
		}
	}

	printk("L-EVK-NRF54L15 UART30: TX=P0.00 RX=P0.01, 115200 8N1\\r\\n");

	while (true) {
		for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
			ret = gpio_pin_toggle_dt(&leds[i]);
			if (ret != 0) {
				return 0;
			}
		}

		leds_on = !leds_on;
		printk("LED P0.02/P0.03: %s\\r\\n", leds_on ? "ON" : "OFF");
		k_msleep(BLINK_PERIOD_MS);
	}
}
