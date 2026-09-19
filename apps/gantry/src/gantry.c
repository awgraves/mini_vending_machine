#include "gantry.h"
#include "steppers.h"
#include <zephyr/kernel.h>

#define STEPPER_MICRO_STEPS_PER_REV                                            \
  (STEPPER_STEPS_PER_REV * STEPPER_MICRO_STEPS_SETTING)

#define Y_AXIS_MM_PER_REV 8 // T8 leadscrew
#define Y_AXIS_MICRO_STEPS_PER_MM                                              \
  (STEPPER_MICRO_STEPS_PER_REV / Y_AXIS_MM_PER_REV)

#define X_AXIS_MM_PER_REV 40 // 20 tooth x 2mm
#define X_AXIS_MICRO_STEPS_PER_MM                                              \
  (STEPPER_MICRO_STEPS_PER_REV / X_AXIS_MM_PER_REV)

/*
  Public API
*/

enum gantry_calibration_state {
  GANTRY_NOT_CALIBRATED,
  GANTRY_CALIBRATING,
  GANTRY_CALIBRATION_COMPLETE,
};

static enum gantry_calibration_state calibration_state;

static struct stepper *stepper_x;
static struct stepper *stepper_y;

int gantry_init(void) {
  int ret;
  if ((ret = steppers_init()) < 0) {
    printk("steppers not ready\n");
    return ret;
  }

  calibration_state = GANTRY_NOT_CALIBRATED;
  stepper_x = stepper_get(STEPPER_X_AXIS);
  stepper_y = stepper_get(STEPPER_Y_AXIS);

  return 0;
}

int gantry_calibrate(void) {
  // TODO:
  /*
    Set state to calibrating
    If limit switch already hit:
      move it back 1 cm
      if switch still 'hit':
         ERR this motor
      else:
         continue
    If limit switch NOT hit:
      run until hit switch
    finally:
      set this as reference point 0
      set state to READY
  */

  stepper_run_until_limit_hit(stepper_x, 50);
  stepper_run_until_limit_hit(stepper_y, 50);

  return 0;
}
