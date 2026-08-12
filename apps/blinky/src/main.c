/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 20

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#define SW0_NODE DT_ALIAS(sw0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(SW0_NODE, gpios);

typedef struct {
  uint32_t cell_one;
  uint32_t cell_two;
} node_spec_t;

#define NODE_DT_SPEC_GET_BY_IDX(node_id, prop, idx)                            \
  {.cell_one = DT_PHA_BY_IDX(node_id, prop, idx, name_of_cell_one),            \
   .cell_two = DT_PHA_BY_IDX_OR(node_id, prop, idx, name_of_cell_two, 0)}

node_spec_t node_a =
    NODE_DT_SPEC_GET_BY_IDX(DT_PATH(node_refs), phandle_array_of_refs, 0);

int main(void) {
  int ret, sw_state, prev_sw_state;
  prev_sw_state = 0;
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
    if (sw_state && sw_state != prev_sw_state) {
      led_state = !led_state;
      ret = gpio_pin_set_dt(&led, led_state);
      if (ret < 0) {
        return 0;
      }
      printf("LED state: %s %d\n", led_state ? "ON" : "OFF",
             led_state ? node_a.cell_one : node_a.cell_two);
    }
    prev_sw_state = sw_state;

    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
