/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/util.h>
#include <zephyr/random/rand32.h>

#define STRIP_NODE		DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)

#define DELAY_TIME K_MSEC(10)
#define TIKCS_MIN 1
#define TICKS_MAX 8
#define BRIGHTNESS_MIN 0x20
#define BRIGHTNESS_MAX 0x1ff
#define DELAY_MIN 3
#define DELAY_MAX 15
#define MAX_SPARKLES 100

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb basic_color = RGB(0x0f, 0x00, 0x16);

struct sparkle {
	struct led_rgb center_color;
	struct led_rgb side_color;
	uint8_t ticks;
	uint8_t now_ticks;
	int pos;
	bool active;
};

static const struct sparkle null_sparkle = {
	.center_color = basic_color,
	.side_color = basic_color,
	.ticks = 0,
	.now_ticks = 0,
	.pos = 0,
	.active = false
};

struct sparkle sparkles[MAX_SPARKLES];

struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

struct led_rgb color_from_brightness(uint16_t brightness, float coef) {
	uint8_t b = MIN(255, brightness);
	uint8_t r = MIN(255, brightness * coef);
	uint8_t g = MIN(255, MAX(0, brightness - 255));
	struct led_rgb res = RGB(r, g, b);
	return res;
}

void place_sparkle_in_array(struct sparkle sp) {
	for (int i=0; i<MAX_SPARKLES; i++){
		if(sparkles[i].active == false){
			memcpy(&sparkles[i], &sp, sizeof(struct sparkle));
			return;
		}
	}
}

void place_sparkle_in_strip(struct sparkle sp) {
	if(0 <= sp.pos - 1 && sp.pos - 1 < STRIP_NUM_PIXELS){
		memcpy(&pixels[sp.pos - 1], &sp.side_color, sizeof(struct led_rgb));
	}
	if(0 <= sp.pos && sp.pos < STRIP_NUM_PIXELS){
		memcpy(&pixels[sp.pos], &sp.center_color, sizeof(struct led_rgb));
	}
	if(0 <= sp.pos + 1 && sp.pos + 1 < STRIP_NUM_PIXELS){
		memcpy(&pixels[sp.pos + 1], &sp.side_color, sizeof(struct led_rgb));
	}
}

int main(void)
{
	int rc;
	uint8_t counter = 0;
	uint32_t rand;
	int i;
	uint8_t rand_ticks;
	uint8_t rand_delay;
	uint16_t rand_brightness;
	uint16_t interm_brightness;
	struct sparkle new_sparkle;

	if (device_is_ready(strip)) {
		LOG_INF("Found LED strip device %s", strip->name);
	} else {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return 0;
	}
    for (i=0; i<MAX_SPARKLES; i++) {
		memcpy(&sparkles[i], &null_sparkle, sizeof(struct sparkle));
	}
	LOG_INF("Displaying pattern on strip");
	float basic_coef = (float)basic_color.r / basic_color.b;
	while (1) {
		for (i=0; i<STRIP_NUM_PIXELS; i++) {
			memcpy(&pixels[i], &basic_color, sizeof(struct led_rgb));
		}
        if (counter == 0) {
			rand = sys_rand32_get();
            rand_ticks = rand;
			rand >>= 8;
			rand_delay = rand;
			rand >>= 8;
			rand_brightness = rand;
			rand_ticks = rand_ticks % (TICKS_MAX - TIKCS_MIN) + TIKCS_MIN;
			rand_delay = rand_delay % (DELAY_MAX - DELAY_MIN) + DELAY_MIN;
			rand_brightness = rand_brightness % (BRIGHTNESS_MAX - BRIGHTNESS_MIN) + BRIGHTNESS_MIN;
			interm_brightness = (rand_brightness + basic_color.b) / 2;
			new_sparkle.center_color = color_from_brightness(rand_brightness, basic_coef);
			new_sparkle.side_color = color_from_brightness(interm_brightness, basic_coef);
			new_sparkle.ticks = rand_ticks;
			new_sparkle.now_ticks = rand_ticks;
			new_sparkle.pos = 0;
			new_sparkle.active = true;
			place_sparkle_in_array(new_sparkle);
			counter = rand_delay;
		}
		for (i=0; i<MAX_SPARKLES; i++) {
			if (sparkles[i].active) {
			    place_sparkle_in_strip(sparkles[i]);
			    sparkles[i].now_ticks--;
			    if (sparkles[i].now_ticks == 0) {
				    sparkles[i].pos++;
                    sparkles[i].now_ticks = sparkles[i].ticks;
					if (sparkles[i].pos > STRIP_NUM_PIXELS) {
						sparkles[i].active = false;
					}
			    }
			}
		}
		rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);

		if (rc) {
			LOG_ERR("couldn't update strip: %d", rc);
		}

		counter--;

		k_sleep(DELAY_TIME);
	}
	return 0;
}
