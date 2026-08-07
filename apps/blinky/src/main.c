/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#define SW0_NODE DT_ALIAS(sw0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

int main(void) {
  int ret, sw_state;
  bool led_state = true;

  if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&btn)) {
    return 0;
  }

  ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  if (ret < 0) {
    return 0;
  }

  ret = gpio_pin_configure_dt(&btn, GPIO_INPUT);
  if (ret < 0) {
    return 0;
  }

  while (1) {
    sw_state = gpio_pin_get_dt(&btn);
    if (ret < 0) {
      return 0;
    }
    if (sw_state) {
      continue;
    }

    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0) {
      return 0;
    }

    led_state = !led_state;
    printf("LED state: %s\n", led_state ? "ON" : "OFF");
    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
