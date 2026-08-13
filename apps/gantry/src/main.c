#include <stdio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20

#define JOYSTICK DT_ALIAS(my_joystick)

static const struct adc_dt_spec adc_channels[2] = {
    ADC_DT_SPEC_GET_BY_IDX(JOYSTICK, 0), ADC_DT_SPEC_GET_BY_IDX(JOYSTICK, 1)};

typedef union {
  struct {
    uint16_t x, y;
  };
  uint16_t raw[2];
} readings_t; // each reading is 12 bits

int main(void) {
  int ret;
  readings_t readings = {0};

  for (int i = 0; i < 2; i++) {
    if (!adc_is_ready_dt(&adc_channels[i])) {
      return 0;
    }

    if (adc_channel_setup_dt(&adc_channels[i]) < 0) {
      return 0;
    }
  }
  struct adc_sequence seq = {.buffer_size = sizeof(uint32_t)};

  while (1) {
    for (int i = 0; i < 2; i++) {
      seq.buffer = &readings.raw[i];

      ret = adc_sequence_init_dt(&adc_channels[i], &seq);
      if (ret < 0) {
        return 0;
      }

      ret = adc_read_dt(&adc_channels[i], &seq);
      if (ret < 0) {
        printf("FAILED to read ADC %s\n", adc_channels[i].dev->name);
      }
    }

    printf("ADC VALs: X=%d Y=%d\n", readings.x, readings.y);

    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
