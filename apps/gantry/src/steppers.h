#pragma once
#include <zephyr/drivers/stepper/stepper_ctrl.h>

#define STEPS_PER_REV 200
#define MICRO_STEPS_SETTING 8
#define MICRO_STEPS_PER_REV (STEPS_PER_REV * MICRO_STEPS_SETTING)

#define Y_AXIS_MM_PER_REV 8 // T8 leadscrew
#define Y_AXIS_MICRO_STEPS_PER_MM (MICRO_STEPS_PER_REV / Y_AXIS_MM_PER_REV)

#define X_AXIS_MM_PER_REV 40 // 20 tooth x 2mm
#define X_AXIS_MICRO_STEPS_PER_MM (MICRO_STEPS_PER_REV / X_AXIS_MM_PER_REV)

struct stepper_run_conf {
  uint8_t speed; // min 1, max 100
  enum stepper_ctrl_direction dir;
};

struct stepper;
struct stepper_handles {
  struct stepper *x;
  struct stepper *y;
};

typedef void (*limit_hit_callback_t)(void);

struct steppers_config {
  struct {
    limit_hit_callback_t limit_hit_x;
    limit_hit_callback_t limit_hit_y;
  } callbacks;
};

int steppers_init(struct steppers_config *conf);
struct stepper_handles get_stepper_handles(void);

int stepper_stop(struct stepper *s);
// TODO: remove stepper_run once positioning logic exists
int stepper_run(struct stepper *s, const struct stepper_run_conf *conf);

/* Limit related funcs */
bool stepper_get_is_at_limit(struct stepper *s);
int stepper_run_until_limit_hit(struct stepper *s, uint8_t speed);
