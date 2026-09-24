#pragma once
#include <zephyr/drivers/stepper/stepper_ctrl.h>

#define STEPPER_STEPS_PER_REV 200
#define STEPPER_MICRO_STEPS_SETTING 8
#define STEPPER_MICRO_STEPS_PER_REV                                            \
  (STEPPER_STEPS_PER_REV * STEPPER_MICRO_STEPS_SETTING)

struct stepper;

#define STEPPER_AXIS_COUNT 2
enum stepper_axis { STEPPER_X_AXIS = 0, STEPPER_Y_AXIS };

struct stepper_run_conf {
  uint8_t speed; // min 1, max 100
  enum stepper_ctrl_direction dir;
};

int steppers_init(void);
struct stepper *stepper_get(enum stepper_axis axis);

void stepper_set_event_sem(struct stepper *s, struct k_sem *sem);

int stepper_stop(struct stepper *s);
// TODO: remove stepper_run once positioning logic exists
int stepper_run(struct stepper *s, const struct stepper_run_conf *conf);

/* Limit related funcs */
bool stepper_get_is_at_limit(struct stepper *s);
int stepper_run_until_limit_hit(struct stepper *s, uint8_t speed);

int stepper_set_speed(struct stepper *s, uint8_t speed);
int stepper_move_steps(struct stepper *s, int32_t steps);
