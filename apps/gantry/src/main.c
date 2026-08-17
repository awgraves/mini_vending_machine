#include "joystick.h"
#include <stdio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20
#define MAX_ADC_READING 4095

#define JOYSTICK DT_ALIAS(my_joystick)
#define RED_LED_NODE DT_ALIAS(red_pwm_led)
#define YELLOW_LED_NODE DT_ALIAS(yellow_pwm_led)

static const struct led_dt_spec red_led = LED_DT_SPEC_GET(RED_LED_NODE);
static const struct led_dt_spec yellow_led = LED_DT_SPEC_GET(YELLOW_LED_NODE);

static const struct joystick_dt_spec joystick = JOYSTICK_DT_SPEC_GET(JOYSTICK);

int main(void) {
  readings_t readings = {0};

  if (!device_is_ready(red_led.dev) || !device_is_ready(yellow_led.dev)) {
    return 0;
  }

  if (!device_is_ready(joystick.dev)) {
    return 0;
  }

  while (1) {
    if (joystick_poll_dt(&joystick, &readings) < 0) {
      printf("ERROR in joystick\n");
    };

    printf("ADC VALs: X=%d Y=%d\n", readings.x, readings.y);

    led_set_brightness_dt(&red_led, readings.x * 100 / MAX_ADC_READING);
    led_set_brightness_dt(&yellow_led, readings.y * 100 / MAX_ADC_READING);

    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
