#include <stdio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20
#define MAX_ADC_READING 4095

#define JOYSTICK DT_ALIAS(my_joystick)
#define RED_LED_NODE DT_ALIAS(red_pwm_led)
#define YELLOW_LED_NODE DT_ALIAS(yellow_pwm_led)

static const struct adc_dt_spec adc_channels[2] = {
    ADC_DT_SPEC_GET_BY_IDX(JOYSTICK, 0), ADC_DT_SPEC_GET_BY_IDX(JOYSTICK, 1)};

static const struct led_dt_spec red_led = LED_DT_SPEC_GET(RED_LED_NODE);
static const struct led_dt_spec yellow_led = LED_DT_SPEC_GET(YELLOW_LED_NODE);

typedef union {
  struct {
    uint16_t x, y;
  };
  uint16_t raw[2];
} readings_t; // each reading is 12 bits

int main(void) {
  int ret;
  readings_t readings = {0};

  if (!device_is_ready(red_led.dev) || !device_is_ready(yellow_led.dev)) {
    return 0;
  }

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

    led_set_brightness_dt(&red_led, readings.x * 100 / MAX_ADC_READING);
    led_set_brightness_dt(&yellow_led, readings.y * 100 / MAX_ADC_READING);

    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
