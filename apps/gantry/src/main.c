#include "joystick.h"
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20
#define MAX_ADC_READING 4095
#define JOYSTICK_DEADZONE_MIN 45
#define JOYSTICK_DEADZONE_MAX 55

#define JOYSTICK DT_ALIAS(my_joystick)

static const struct joystick_dt_spec joystick = JOYSTICK_DT_SPEC_GET(JOYSTICK);

static const struct device *stepper_driver =
    DEVICE_DT_GET(DT_ALIAS(stepper_driver));
static const struct device *stepper_ctrl =
    DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));

static inline uint8_t normalize(uint16_t raw) {
  uint8_t tmp = raw * 100 / MAX_ADC_READING;
  if (tmp > JOYSTICK_DEADZONE_MIN && tmp < JOYSTICK_DEADZONE_MAX) {
    return 50; // pin to the 'deadzone' number
  } else if (tmp > 99) {
    return 100; // this particular joystick is unstable around 99
  }

  return tmp;
}

#define MIN_NS_INTERVAL 180000
#define NS_INTERVAL_PER_TICK (MIN_NS_INTERVAL / 50)
#define NS_SLOWEST (MIN_NS_INTERVAL + (NS_INTERVAL_PER_TICK * 50))

static inline uint32_t get_speed(uint8_t normalized_val) {
  return NS_SLOWEST - (abs(normalized_val - 50) * NS_INTERVAL_PER_TICK);
}

static uint8_t normalized_x;

static void stepper_update_output(readings_t *readings) {
  uint8_t next_x = normalize(readings->x);

  if (next_x != normalized_x) {
    stepper_ctrl_stop(stepper_ctrl);

    if (next_x > JOYSTICK_DEADZONE_MAX || next_x < JOYSTICK_DEADZONE_MIN) {
      uint32_t speed = get_speed(next_x);
      printf("setting to ns val: %d\n", speed);
      int32_t ret = stepper_ctrl_set_microstep_interval(stepper_ctrl, speed);
      if (ret < 0) {
        printf("BAD INTERVAL %d\n", ret);
      }

      stepper_ctrl_run(stepper_ctrl, next_x < 50
                                         ? STEPPER_CTRL_DIRECTION_NEGATIVE
                                         : STEPPER_CTRL_DIRECTION_POSITIVE);
    }
  }

  normalized_x = next_x;
}

int main(void) {
  readings_t readings = {0};

  if (!device_is_ready(joystick.dev)) {
    return 0;
  }

  if (!device_is_ready(stepper_ctrl)) {
    return 0;
  }

  if (stepper_enable(stepper_driver) < 0) {
    while (1) {
      printf("COULD NOT ENABLE\n");
      k_msleep(SLEEP_TIME_MS);
    }
    return 0;
  }

  while (1) {
    if (joystick_poll_dt(&joystick, &readings) < 0) {
      printf("ERROR in joystick\n");
    };

    stepper_update_output(&readings);

    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
};
