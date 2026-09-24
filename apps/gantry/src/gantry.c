#include "gantry.h"
#include "steppers.h"
#include <zephyr/kernel.h>

#define STEPPER_MICRO_STEPS_PER_REV                                            \
  (STEPPER_STEPS_PER_REV * STEPPER_MICRO_STEPS_SETTING)

#define Y_AXIS_MM_PER_REV 8 // T8 leadscrew
#define Y_AXIS_MICRO_STEPS_PER_MM                                              \
  (STEPPER_MICRO_STEPS_PER_REV / Y_AXIS_MM_PER_REV)
#define Y_AXIS_MAX_DISTANCE_IN_MM 180

#define X_AXIS_MM_PER_REV 40 // 20 tooth x 2mm
#define X_AXIS_MICRO_STEPS_PER_MM                                              \
  (STEPPER_MICRO_STEPS_PER_REV / X_AXIS_MM_PER_REV)
#define X_AXIS_MAX_DISTANCE_IN_MM 305

#define HOMING_SPEED 20

struct coords_in_mm {
  uint32_t x;
  uint32_t y;
};

#define COL_1 305
#define COL_2 205
#define COL_3 105
#define TOP_ROW 170
#define BOTTOM_ROW 50

const struct coords_in_mm positions[NUM_GANTRY_POSITIONS] = {
    [POS_HOME] = {.x = 0, .y = 0},
    [POS_A] = {.x = COL_1, .y = TOP_ROW},
    [POS_B] = {.x = COL_2, .y = TOP_ROW},
    [POS_C] = {.x = COL_3, .y = TOP_ROW},
    [POS_D] = {.x = COL_1, .y = BOTTOM_ROW},
    [POS_E] = {.x = COL_2, .y = BOTTOM_ROW},
    [POS_F] = {.x = COL_3, .y = BOTTOM_ROW}};

struct axis {
  struct k_sem event_sem;
  struct stepper *stepper;
  uint32_t micro_steps_per_mm;
  uint32_t max_distance_in_mm;
};

enum gantry_calibration_state {
  NOT_CALIBRATED,
  CALIBRATING,
  CALIBRATION_ERR,
  CALIBRATION_COMPLETE,
};

struct gantry {
  enum gantry_calibration_state calibration_state;
  struct axis x;
  struct axis y;
};

static struct gantry gantry;

/*
  Helpers
*/

static inline int32_t millimeters_to_steps(struct axis *a, int32_t mm) {
  return a->micro_steps_per_mm * mm;
}

// note: must be sure to call this only after calibration
static inline int32_t get_curr_axis_pos_in_mm(struct axis *a) {
  int32_t pos;
  stepper_read_curr_step_count(a->stepper, &pos);
  return pos / a->micro_steps_per_mm;
}

const struct coords_in_mm *get_coords_for_position(enum gantry_pos pos) {
  return &positions[pos];
}

bool axis_calibration(struct axis *a) {
  /*
    If the switch is already engaged, back things up slightly,
    then assert it is no longer engaged before proceeding.
    This adds extra assurance that an engaged switch is an accurate reading.
  */
  if (stepper_get_is_at_limit(a->stepper)) {
    stepper_set_speed(a->stepper, HOMING_SPEED);
    stepper_move_steps(a->stepper, millimeters_to_steps(a, 10));
    k_sem_take(&a->event_sem, K_SECONDS(2));
    k_msleep(50);
    if (stepper_get_is_at_limit(a->stepper)) {
      return false;
    }
  }

  stepper_run_until_limit_hit(a->stepper, HOMING_SPEED);
  k_sem_take(&a->event_sem, K_SECONDS(10));
  if (!stepper_get_is_at_limit(a->stepper)) {
    return false;
  }

  return true;
}

/*
  Public API
*/

int gantry_init(void) {
  int ret;
  if ((ret = steppers_init()) < 0) {
    printk("steppers not ready\n");
    return ret;
  }

  gantry.calibration_state = NOT_CALIBRATED;
  gantry.x = (struct axis){
      .stepper = stepper_get(STEPPER_X_AXIS),
      .micro_steps_per_mm = X_AXIS_MICRO_STEPS_PER_MM,
      .max_distance_in_mm = X_AXIS_MAX_DISTANCE_IN_MM,
  };
  gantry.y = (struct axis){.stepper = stepper_get(STEPPER_Y_AXIS),
                           .micro_steps_per_mm = Y_AXIS_MICRO_STEPS_PER_MM,
                           .max_distance_in_mm = Y_AXIS_MAX_DISTANCE_IN_MM};

  k_sem_init(&gantry.x.event_sem, 0, 1);
  k_sem_init(&gantry.y.event_sem, 0, 1);

  stepper_set_event_sem(gantry.x.stepper, &gantry.x.event_sem);
  stepper_set_event_sem(gantry.y.stepper, &gantry.y.event_sem);

  return 0;
}

int gantry_calibrate(void) {
  gantry.calibration_state = CALIBRATING;

  if (axis_calibration(&gantry.x) && axis_calibration(&gantry.y)) {
    gantry.calibration_state = CALIBRATION_COMPLETE;
    return 0;
  } else {
    gantry.calibration_state = CALIBRATION_ERR;
    return -1;
  }
}

int gantry_move_to_pos(enum gantry_pos pos) {
  if (gantry.calibration_state != CALIBRATION_COMPLETE) {
    return -EIO;
  }
  const struct coords_in_mm *target = get_coords_for_position(pos);
  if (target->x > gantry.x.max_distance_in_mm ||
      target->y > gantry.y.max_distance_in_mm) {
    return -EINVAL;
  }

  int32_t x_move = target->x - get_curr_axis_pos_in_mm(&gantry.x);
  if (target->x == 0) {
    stepper_run_until_limit_hit(gantry.x.stepper, 50);
    k_sem_take(&gantry.x.event_sem, K_FOREVER);
  } else {
    stepper_set_speed(gantry.x.stepper, 50);
    stepper_move_steps(gantry.x.stepper,
                       millimeters_to_steps(&gantry.x, x_move));
    // TODO: better err handling
    k_sem_take(&gantry.x.event_sem, K_FOREVER);
  }

  int32_t y_move = target->y - get_curr_axis_pos_in_mm(&gantry.y);
  if (target->y == 0) {
    stepper_run_until_limit_hit(gantry.y.stepper, 50);
    k_sem_take(&gantry.y.event_sem, K_FOREVER);
  } else {
    stepper_set_speed(gantry.y.stepper, 50);
    stepper_move_steps(gantry.y.stepper,
                       millimeters_to_steps(&gantry.y, y_move));
    // TODO: better err handling
    k_sem_take(&gantry.y.event_sem, K_FOREVER);
  }

  return 0;
}
