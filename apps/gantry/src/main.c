/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20

#define JOYSTICK DT_ALIAS(my_joystick)

static const struct adc_dt_spec my_adc_spec = ADC_DT_SPEC_GET(JOYSTICK);

int main(void) {
  int ret;

  uint32_t buf = 0;
  struct adc_sequence seq = {.buffer = &buf, .buffer_size = sizeof(buf)};

  ret = adc_sequence_init_dt(&my_adc_spec, &seq);
  if (ret < 0) {
    return 0;
  }

  while (1) {
    ret = adc_read_dt(&my_adc_spec, &seq);
    if (ret < 0) {
      printf("Could not read ADC: %d\n", ret);
    } else {
      printf("ADC VAL: %d\n", buf);
    }

    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
