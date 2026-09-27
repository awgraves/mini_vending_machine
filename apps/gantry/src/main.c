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

uint8_t cmd_idx;
K_SEM_DEFINE(blocking_cmd_sem, 0, 1);

void gantry_cb(const struct gantry_event *ev) {
  if (ev->type != GANTRY_MOVE_COMPLETED) {
    printf("Something went wrong. Err code %d\n",
           (enum gantry_err_type)ev->data);
  }
  k_sem_give(&blocking_cmd_sem);
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

  gantry_register_event_cb(gantry_cb);

  int ret;
  struct gantry_cmd cmd;
  for (cmd_idx = 0; cmd_idx < NUM_GANTRY_POSITIONS; cmd_idx++) {
    cmd.type = GANTRY_MOVE;
    cmd.target_pos = (enum gantry_pos)cmd_idx;

    ret = gantry_cmd_send(&cmd);
    if (ret < 0) {
      printf("gantry cmd failed!\n");
      break;
    }
    k_sem_take(&blocking_cmd_sem, K_FOREVER);

    k_msleep(1000);
  }
  cmd.target_pos = POS_HOME;
  gantry_cmd_send(&cmd);
  k_sem_take(&blocking_cmd_sem, K_FOREVER);

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
