#pragma once
#include <zephyr/drivers/stepper/stepper_ctrl.h>

struct stepper_run_t {
  uint8_t speed; // min 1, max 100
  enum stepper_ctrl_direction dir;
};

int steppers_init(void);

int steppers_x_stop(void);
int steppers_x_run(struct stepper_run_t *conf);
