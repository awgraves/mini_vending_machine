#pragma once
#include <zephyr/drivers/adc.h>

typedef union {
  struct {
    int8_t x, y; // -100 to +100
  };
  int8_t arr[2];
} readings_t;

struct joystick_api {
  int (*poll)(const struct device *dev, readings_t *readings);
};

struct joystick_config {
  struct adc_dt_spec chans[2];
  uint32_t id;
};

struct joystick_dt_spec {
  const struct device *dev;
};

#define JOYSTICK_DT_SPEC_GET(node_id) {.dev = DEVICE_DT_GET(node_id)}

static inline int joystick_poll_dt(const struct joystick_dt_spec *spec,
                                   readings_t *readings) {
  return ((const struct joystick_api *)spec->dev->api)
      ->poll(spec->dev, readings);
};
