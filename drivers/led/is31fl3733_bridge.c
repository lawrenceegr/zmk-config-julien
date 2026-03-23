/*
 * Copyright (c) 2026 Lawrence Langat <lawrencelangatmi@gmail.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bridges two IS31FL3733 LED matrix devices into a single led_strip device
 * for ZMK RGB underglow.
 */

#define DT_DRV_COMPAT zmk_is31fl3733_bridge

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(is31fl3733_bridge, CONFIG_LED_LOG_LEVEL);

#define IS31_COLS     16
#define IS31_LEDS     192

#define DRV1_GROUPS   4
#define DRV1_COLS     16
#define DRV1_PIXELS   63

#define DRV2_GROUPS   2
#define DRV2_COLS     14
#define DRV2_PIXELS   25

struct bridge_cfg {
	uint32_t chain_length;
};

static size_t bridge_length(const struct device *dev)
{
	return ((const struct bridge_cfg *)dev->config)->chain_length;
}

static int bridge_update_rgb(const struct device *dev,
				 struct led_rgb *pixels, size_t num_pixels)
{
	const struct device *drv1 = DEVICE_DT_GET(DT_NODELABEL(led_driver1));
	const struct device *drv2 = DEVICE_DT_GET(DT_NODELABEL(led_driver2));
	uint8_t pwm1[IS31_LEDS] = {0};
	uint8_t pwm2[IS31_LEDS] = {0};
	size_t n = MIN(num_pixels, (size_t)(DRV1_PIXELS + DRV2_PIXELS));
	int ret;

	for (size_t i = 0; i < MIN(n, (size_t)DRV1_PIXELS); i++) {
		int g = i / DRV1_COLS;
		int c = i % DRV1_COLS;

		pwm1[(g * 3 + 0) * IS31_COLS + c] = pixels[i].r;
		pwm1[(g * 3 + 1) * IS31_COLS + c] = pixels[i].g;
		pwm1[(g * 3 + 2) * IS31_COLS + c] = pixels[i].b;
	}

	for (size_t i = DRV1_PIXELS; i < n; i++) {
		int l = i - DRV1_PIXELS;
		int g = l / DRV2_COLS;
		int c = l % DRV2_COLS;

		pwm2[(g * 3 + 0) * IS31_COLS + c] = pixels[i].r;
		pwm2[(g * 3 + 1) * IS31_COLS + c] = pixels[i].g;
		pwm2[(g * 3 + 2) * IS31_COLS + c] = pixels[i].b;
	}

	ret = led_write_channels(drv1, 0, IS31_LEDS, pwm1);
	if (ret < 0) {
		LOG_ERR("drv1 write failed: %d", ret);
		return ret;
	}

	ret = led_write_channels(drv2, 0, IS31_LEDS, pwm2);
	if (ret < 0) {
		LOG_ERR("drv2 write failed: %d", ret);
	}

	return ret;
}

static const struct led_strip_driver_api bridge_api = {
	.update_rgb = bridge_update_rgb,
	.length     = bridge_length,
};

#define BRIDGE_DEFINE(n)                                                    \
	static const struct bridge_cfg bridge_cfg_##n = {                   \
		.chain_length = DT_INST_PROP(n, chain_length),              \
	};                                                                  \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &bridge_cfg_##n,        \
				  POST_KERNEL, 90, &bridge_api);

DT_INST_FOREACH_STATUS_OKAY(BRIDGE_DEFINE)
