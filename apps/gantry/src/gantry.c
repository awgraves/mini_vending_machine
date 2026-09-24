#include "gantry.h"
#include "steppers.h"
#include <zephyr/kernel.h>

#define STEPPER_MICRO_STEPS_PER_REV                                            \
  (STEPPER_STEPS_PER_REV * STEPPER_MICRO_STEPS_SETTING)

#define Y_AXIS_MM_PER_REV 8 // T8 leadscrew
#define Y_AXIS_MICRO_STEPS_PER_MM                                              \
  (STEPPER_MICRO_STEPS_PER_REV / Y_AXIS_MM_PER_REV)
#define Y_AXIS_MAX_DISTANCE_IN_MM 80

#define X_AXIS_MM_PER_REV 40 // 20 tooth x 2mm
#define X_AXIS_MICRO_STEPS_PER_MM                                              \
  (STEPPER_MICRO_STEPS_PER_REV / X_AXIS_MM_PER_REV)
#define X_AXIS_MAX_DISTANCE_IN_MM 305

#define HOMING_SPEED 20

struct axis {
  struct k_sem event_sem;
  struct stepper *stepper;
  uint32_t micro_steps_per_mm;
  uint32_t max_distance_in_mm;
  uint32_t current_pos_in_mm;
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

static inline int32_t millimeters_to_steps(struct axis *a, int32_t mm){
  return a->micro_steps_per_mm * mm;
}

bool axis_calibration(struct axis *a){  
  /*
    If the switch is already engaged, back things up slightly,
    then assert it is no longer engaged before proceeding.
    This adds extra assurance that an engaged switch is an accurate reading.
  */
  if (stepper_get_is_at_limit(a->stepper)){
    stepper_set_speed(a->stepper, HOMING_SPEED);
    stepper_move_steps(a->stepper, millimeters_to_steps(a, 10));
    k_sem_take(&a->event_sem, K_SECONDS(2));
    k_msleep(50);
    if (stepper_get_is_at_limit(a->stepper)){
      return false;
    }
  }

  stepper_run_until_limit_hit(a->stepper, HOMING_SPEED);
  k_sem_take(&a->event_sem, K_SECONDS(10));
  if (!stepper_get_is_at_limit(a->stepper)){
    return false;
  }

  a->current_pos_in_mm = 0;
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
  gantry.y = (struct axis){
    .stepper = stepper_get(STEPPER_Y_AXIS),
    .micro_steps_per_mm = Y_AXIS_MICRO_STEPS_PER_MM,
    .max_distance_in_mm = Y_AXIS_MAX_DISTANCE_IN_MM
  };

  k_sem_init(&gantry.x.event_sem, 0, 1);
  k_sem_init(&gantry.y.event_sem, 0, 1);

  stepper_set_event_sem(gantry.x.stepper, &gantry.x.event_sem);
  stepper_set_event_sem(gantry.y.stepper, &gantry.y.event_sem);
  
  return 0;
}

int gantry_calibrate(void) {
  gantry.calibration_state = CALIBRATING;

  if (axis_calibration(&gantry.x) && axis_calibration(&gantry.y)){
    gantry.calibration_state = CALIBRATION_COMPLETE;
    return 0;
  } else {    
    gantry.calibration_state = CALIBRATION_ERR;
    return -1;
  }
}
