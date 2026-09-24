#include "gantry.h"
#include "joystick.h"
#include "steppers.h"
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/drivers/adc.h>

#define SLEEP_TIME_MS 20

#define JOYSTICK DT_ALIAS(my_joystick)

static const struct joystick_dt_spec joystick = JOYSTICK_DT_SPEC_GET(JOYSTICK);

void loop_message(const char *msg) {
  while (1) {
    printf("%s\n", msg);
    k_msleep(SLEEP_TIME_MS);
  }
}

int main(void) {
  readings_t readings = {0};

  if (!device_is_ready(joystick.dev)) {
    loop_message("joystick not ready");
    return 0;
  }

  gantry_init();

  int8_t prev_readings[2] = {0};
  struct stepper_run_conf stepper_conf;

  struct stepper *steppers[2] = {stepper_get(STEPPER_X_AXIS),
                                 stepper_get(STEPPER_Y_AXIS)};

  k_msleep(1000);
  gantry_calibrate();

  int ret;
  for (int i = 1; i < 7; i++){
    ret = gantry_move_to_pos((enum gantry_pos)i);
    if (ret < 0){
      break;
    }
    k_msleep(1000);
  }

  gantry_move_to_pos(POS_HOME);

  while (1) {
    if (joystick_poll_dt(&joystick, &readings) < 0) {
      printf("ERROR in joystick\n");
    };

    for (int i = 0; i < 2; i++) {
      int8_t reading = readings.arr[i];
      if (reading != prev_readings[i]) {
        if (reading == 0) {
          stepper_stop(steppers[i]);
        } else {
          uint8_t absolute = abs(reading);
          stepper_conf.speed = absolute;
          stepper_conf.dir = reading > 0 ? STEPPER_CTRL_DIRECTION_POSITIVE
                                         : STEPPER_CTRL_DIRECTION_NEGATIVE;
          stepper_run(steppers[i], &stepper_conf);
        }
        prev_readings[i] = reading;
      }
    }

    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
};
