#pragma once
#include <zephyr/drivers/adc.h>

typedef union {
  struct {
    uint16_t x, y;
  };
  uint16_t raw[2];
} readings_t; // each reading is 12 bits

struct joystick_api {
  int (*poll)(const struct device *dev, readings_t *readings);
};

struct joystick_config {
  struct adc_dt_spec chans[2];
  uint32_t id;
};
