#include <zephyr/logging/log.h>

#include "joystick.h"

#define DT_DRV_COMPAT joystick

LOG_MODULE_REGISTER(joystick);

#define MAX_ADC_READING 4095 // assumes 12 bit ADC
#define JOYSTICK_DEADZONE_MIN 45
#define JOYSTICK_DEADZONE_MAX 55

/*
  Forward declarations
*/

static int joystick_init(const struct device *dev);
static int joystick_poll(const struct device *dev, readings_t *readings);

/*
  Helpers
*/

static int joystick_init(const struct device *dev) {
  const struct joystick_config *cfg =
      (const struct joystick_config *)dev->config;

  for (int i = 0; i < 2; i++) {
    if (!adc_is_ready_dt(&cfg->chans[i])) {
      LOG_ERR("Joystick %s chan is not ready", i == 0 ? "X" : "Y");
      return -ENODEV;
    }

    if (adc_channel_setup_dt(&cfg->chans[i]) < 0) {
      LOG_ERR("Joystick %s chan could not be set up", i == 0 ? "X" : "Y");
      return -ENODEV;
    }
  }

  return 0;
}

// normalize raw 12 bit ADC reading of 0 to MAX_ADC
// and convert to number between -100 and +100
static int8_t normalize(uint16_t raw) {
  int32_t tmp =
      raw * 100 / MAX_ADC_READING; // first convert to 0 - 100 for easier math

  if (tmp > JOYSTICK_DEADZONE_MIN && tmp < JOYSTICK_DEADZONE_MAX) {
    return 0; // pin to the 'deadzone' number
  } else if (tmp >= 99) {
    return 100; // this particular joystick is unstable around 99
  }

  return ((tmp - 50) << 1);
}

/*
  Public API
*/

static int joystick_poll(const struct device *dev, readings_t *readings) {
  int ret = 0;
  uint16_t raw[2];

  const struct joystick_config *cfg =
      (const struct joystick_config *)dev->config;

  struct adc_sequence seq = {.buffer_size = sizeof(uint16_t)};

  for (int i = 0; i < 2; i++) {
    seq.buffer = &raw[i];

    ret = adc_sequence_init_dt(&cfg->chans[i], &seq);
    if (ret < 0) {
      LOG_ERR("FAILED to init adc sequence %s\n", cfg->chans[i].dev->name);
      return ret;
    }

    ret = adc_read_dt(&cfg->chans[i], &seq);
    if (ret < 0) {
      LOG_ERR("FAILED to read ADC %s\n", cfg->chans[i].dev->name);
      return ret;
    }

    readings->arr[i] = normalize(raw[i]);
  }

  return ret;
}

/*
  DT handling
*/

static const struct joystick_api joystick_api_funcs = {
    .poll = joystick_poll,
};

#define JOYSTICK_DEFINE(inst)                                                  \
  static const struct joystick_config joystick_config_##inst = {               \
      .chans =                                                                 \
          {                                                                    \
              ADC_DT_SPEC_GET_BY_NAME(DT_INST(inst, joystick), x),             \
              ADC_DT_SPEC_GET_BY_NAME(DT_INST(inst, joystick), y),             \
          },                                                                   \
      .id = inst};                                                             \
                                                                               \
  DEVICE_DT_INST_DEFINE(inst, joystick_init, NULL, NULL,                       \
                        &joystick_config_##inst, POST_KERNEL, 999,             \
                        &joystick_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(JOYSTICK_DEFINE)
