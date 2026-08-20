#include "joystick.h"
#include "steppers.h"
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/adc.h>

#define SLEEP_TIME_MS 20

#define JOYSTICK DT_ALIAS(my_joystick)

static const struct joystick_dt_spec joystick = JOYSTICK_DT_SPEC_GET(JOYSTICK);

int main(void) {
  readings_t readings = {0};

  if (!device_is_ready(joystick.dev)) {
    return 0;
  }

  int ret;
  if ((ret = steppers_init()) < 0) {
    return 0;
  }

  int8_t x_read_prev = 0;
  struct stepper_run_t x_conf = {0};

  while (1) {
    if (joystick_poll_dt(&joystick, &readings) < 0) {
      printf("ERROR in joystick\n");
    };

    if (readings.x != x_read_prev) {
      printf("%d\n", readings.x);

      if (readings.x == 0) {
        steppers_x_stop();
      } else {
        uint8_t absolute = abs(readings.x);
        x_conf.speed = absolute;
        x_conf.dir = readings.x > 0 ? STEPPER_CTRL_DIRECTION_POSITIVE
                                    : STEPPER_CTRL_DIRECTION_NEGATIVE;
        steppers_x_run(&x_conf);
      }
      x_read_prev = readings.x;
    }

    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
};
