#pragma once
#include <zephyr/drivers/stepper/stepper_ctrl.h>

struct stepper_run_conf {
  uint8_t speed; // min 1, max 100
  enum stepper_ctrl_direction dir;
};

struct stepper;
struct stepper_handles {
  struct stepper *x;
  struct stepper *y;
};

int steppers_init(void);
struct stepper_handles get_stepper_handles(void);

int stepper_stop(struct stepper *s);
int stepper_run(struct stepper *s, const struct stepper_run_conf *conf);
