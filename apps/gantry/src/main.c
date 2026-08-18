#include "joystick.h"
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20
#define MAX_ADC_READING 4095

#define JOYSTICK DT_ALIAS(my_joystick)
#define RED_LED_NODE DT_ALIAS(red_pwm_led)
#define YELLOW_LED_NODE DT_ALIAS(yellow_pwm_led)

static const struct led_dt_spec red_led = LED_DT_SPEC_GET(RED_LED_NODE);
static const struct led_dt_spec yellow_led = LED_DT_SPEC_GET(YELLOW_LED_NODE);

static const struct joystick_dt_spec joystick = JOYSTICK_DT_SPEC_GET(JOYSTICK);

static const struct device *stepper_driver =
    DEVICE_DT_GET(DT_ALIAS(stepper_driver));
static const struct device *stepper_ctrl =
    DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));

static inline uint8_t normalize(uint16_t raw) {
  return raw * 100 / MAX_ADC_READING;
}

int main(void) {
  readings_t readings = {0};

  if (!device_is_ready(red_led.dev) || !device_is_ready(yellow_led.dev)) {
    return 0;
  }

  if (!device_is_ready(joystick.dev)) {
    return 0;
  }

  if (!device_is_ready(stepper_ctrl)) {
    return 0;
  }

  if (stepper_ctrl_set_reference_position(stepper_ctrl, 0) < 0) {
    // return 0;
  }
  if (stepper_ctrl_set_microstep_interval(stepper_ctrl, 500000) < 0) {
    // return 0;
  }
  if (stepper_enable(stepper_driver) < 0) {
    while (1) {
      printf("COULD NOT ENABLE\n");
      k_msleep(SLEEP_TIME_MS);
    }
    return 0;
  }

  int ret = stepper_ctrl_run(stepper_ctrl, STEPPER_CTRL_DIRECTION_POSITIVE);
  if (ret < 0) {
    while (1) {
      printf("COULD NOT RUN %d\n", ret);
      k_msleep(SLEEP_TIME_MS);
    }
    return 0;
  }

  uint8_t prev_speed = 0;

#define MIN_NS_INTERVAL 200000

  while (1) {
    if (joystick_poll_dt(&joystick, &readings) < 0) {
      printf("ERROR in joystick\n");
    };

    led_set_brightness_dt(&red_led, normalize(readings.x));
    led_set_brightness_dt(&yellow_led, normalize(readings.y));

    uint8_t x_speed = normalize(readings.x);

    if (x_speed != prev_speed) {
      stepper_ctrl_stop(stepper_ctrl);
      if (x_speed > 55 || x_speed < 45) {
        int val = (51 - abs(x_speed - 50)) * 20000;
        printf("setting to ns val: %d\n", val);
        int final_val = val < MIN_NS_INTERVAL ? MIN_NS_INTERVAL : val;
        ret = stepper_ctrl_set_microstep_interval(stepper_ctrl, final_val);
        if (ret < 0) {
          printf("BAD INTERVAL %d\n", ret);
        }

        stepper_ctrl_run(stepper_ctrl, x_speed < 50
                                           ? STEPPER_CTRL_DIRECTION_NEGATIVE
                                           : STEPPER_CTRL_DIRECTION_POSITIVE);
      }

      prev_speed = x_speed;
    }

    k_msleep(SLEEP_TIME_MS);
  }
  return 0;
}
