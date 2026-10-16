#include "gantry.h"
#include "steppers.h"
#include "zephyr/sys/clock.h"
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
#define X_AXIS_MAX_DISTANCE_IN_MM 310

#define HOMING_SPEED_Y 20
#define MAX_SPEED_Y 60
#define HOMING_SPEED_X 10
#define MAX_SPEED_X 50

struct coords_in_mm {
  uint32_t x;
  uint32_t y;
};

#define COL_1 310
#define COL_2 220
#define COL_3 130
#define TOP_ROW 175
#define BOTTOM_ROW 0

BUILD_ASSERT(COL_1 <= X_AXIS_MAX_DISTANCE_IN_MM, "COL_1 exceeds X travel");
BUILD_ASSERT(COL_2 <= X_AXIS_MAX_DISTANCE_IN_MM, "COL_2 exceeds X travel");
BUILD_ASSERT(COL_3 <= X_AXIS_MAX_DISTANCE_IN_MM, "COL_3 exceeds X travel");
BUILD_ASSERT(TOP_ROW <= Y_AXIS_MAX_DISTANCE_IN_MM, "TOP_ROW exceeds Y travel");
BUILD_ASSERT(BOTTOM_ROW <= Y_AXIS_MAX_DISTANCE_IN_MM,
             "BOTTOM_ROW exceeds Y travel");

static const struct coords_in_mm positions[NUM_GANTRY_POSITIONS] = {
    [POS_HOME] = {.x = 0, .y = 0},
    [POS_A] = {.x = COL_1, .y = TOP_ROW},
    [POS_B] = {.x = COL_2, .y = TOP_ROW},
    [POS_C] = {.x = COL_3, .y = TOP_ROW},
    [POS_D] = {.x = COL_1, .y = BOTTOM_ROW},
    [POS_E] = {.x = COL_2, .y = BOTTOM_ROW},
    [POS_F] = {.x = COL_3, .y = BOTTOM_ROW}};

static struct k_event gantry_events;
#define EVENT_X_DONE BIT(0)
#define EVENT_Y_DONE BIT(1)
#define EVENT_ABORT BIT(2)

struct axis {
  struct stepper *stepper;
  uint32_t micro_steps_per_mm;
  uint32_t max_distance_in_mm;
  uint32_t done_bit;
};

struct gantry {
  bool calibrated;
  struct axis x;
  struct axis y;
  atomic_t busy;
};

static struct gantry gantry;

/*
  Helpers
*/

// -------------------
// --- Conversions ---
// -------------------

static inline int32_t millimeters_to_steps(struct axis *a, int32_t mm) {
  return a->micro_steps_per_mm * mm;
}

// note: must be sure to call this only after calibration
static inline int32_t get_curr_axis_pos_in_mm(struct axis *a) {
  int32_t pos;
  stepper_read_curr_step_count(a->stepper, &pos);
  return pos / a->micro_steps_per_mm;
}

static inline const struct coords_in_mm *
get_coords_for_position(enum gantry_pos pos) {
  return &positions[pos];
}

// --------------
// ---- Wait ----
// --------------
static enum gantry_result wait_for_axes(uint32_t axes_mask, int timeout_s) {
  uint32_t done = 0;

  k_timepoint_t deadline = sys_timepoint_calc(K_SECONDS(timeout_s));

  enum gantry_result res = GANTRY_OK;

  while ((done & axes_mask) != axes_mask) {
    uint32_t ev =
        k_event_wait(&gantry_events, (~done & axes_mask) | EVENT_ABORT, false,
                     sys_timepoint_timeout(deadline));

    if (ev == 0) {
      res = GANTRY_ERR_TIMEOUT;
      break;
    }

    if (ev & EVENT_ABORT) {
      res = GANTRY_HALTED;
      break;
    }

    done |= ev & axes_mask;
  }

  k_event_clear(&gantry_events, EVENT_X_DONE | EVENT_Y_DONE | EVENT_ABORT);

  return res;
}

// -----------------------------
// ---- Movement Primitives ----
// -----------------------------
static void axis_move_to_mm_pos(struct axis *a, int target_mm, uint8_t speed) {
  int32_t move_delta = target_mm - get_curr_axis_pos_in_mm(a);

  if (target_mm == 0) {
    stepper_run_until_limit_hit(a->stepper, speed);
  } else {
    stepper_set_speed(a->stepper, speed);
    stepper_move_steps(a->stepper, millimeters_to_steps(a, move_delta));
  }
}

static enum gantry_result axis_move_and_wait(struct axis *a, int target_mm,
                                             uint8_t speed, int timeout_s) {
  axis_move_to_mm_pos(a, target_mm, speed);
  return wait_for_axes(a->done_bit, timeout_s);
}

// ---------------------
// ---- Calibration ----
// ---------------------

static enum gantry_result axis_calibrate(struct axis *a) {
  /*
    If the switch is already engaged, back things up slightly,
    then assert it is no longer engaged before proceeding.
    This adds extra assurance that an engaged switch is an accurate reading.
  */
  struct stepper *s = a->stepper;
  uint8_t speed = (a == &gantry.x) ? HOMING_SPEED_X : HOMING_SPEED_Y;

  enum gantry_result res;

  if (stepper_get_is_at_limit(s)) {
    stepper_set_speed(s, speed);
    stepper_move_steps(s, millimeters_to_steps(a, 10));
    res = wait_for_axes(a->done_bit, 5);
    if (res != GANTRY_OK) {
      return res;
    }
    k_msleep(100);
    if (stepper_get_is_at_limit(s)) {
      return GANTRY_ERR_SW_FAILURE;
    }
  }

  stepper_run_until_limit_hit(s, speed);
  res = wait_for_axes(a->done_bit, 30);
  if (res != GANTRY_OK) {
    return res;
  }
  if (!stepper_get_is_at_limit(s)) {
    return GANTRY_ERR_SW_FAILURE;
  }

  return GANTRY_OK;
}

static enum gantry_result gantry_calibrate(void) {
  enum gantry_result res = axis_calibrate(&gantry.y);
  if (res == GANTRY_OK) {
    res = axis_calibrate(&gantry.x);
  }

  gantry.calibrated = res == GANTRY_OK;
  return res;
}

// ---------------------------
// ---- Movement handlers ----
// ---------------------------

static enum gantry_result gantry_move_to_home(void) {
  struct coords_in_mm home = positions[POS_HOME];

  enum gantry_result res =
      axis_move_and_wait(&gantry.y, home.y, HOMING_SPEED_Y, 30);
  if (res == GANTRY_OK) {
    res = axis_move_and_wait(&gantry.x, home.x, HOMING_SPEED_X, 30);
  }

  return res;
}

static enum gantry_result gantry_move_to_cubby(enum gantry_pos pos) {
  enum gantry_result res;
  if (!gantry.calibrated) {
    res = gantry_calibrate();
    if (res != GANTRY_OK) {
      return res; // prevent originally requested move
    }
    k_msleep(500); // smoother movement with slight pause
  }

  const struct coords_in_mm *target = get_coords_for_position(pos);

  axis_move_to_mm_pos(&gantry.x, target->x, MAX_SPEED_X);
  axis_move_to_mm_pos(&gantry.y, target->y, MAX_SPEED_Y);

  res = wait_for_axes(gantry.x.done_bit | gantry.y.done_bit, 15);
  return res;
}

/*
  Public API
*/

int gantry_init(void) {
  int ret;
  if ((ret = steppers_init()) < 0) {
    return ret;
  }

  gantry.calibrated = false;
  atomic_clear(&gantry.busy);

  gantry.x = (struct axis){
      .stepper = stepper_get(STEPPER_X_AXIS),
      .micro_steps_per_mm = X_AXIS_MICRO_STEPS_PER_MM,
      .max_distance_in_mm = X_AXIS_MAX_DISTANCE_IN_MM,
      .done_bit = EVENT_X_DONE,
  };
  gantry.y = (struct axis){
      .stepper = stepper_get(STEPPER_Y_AXIS),
      .micro_steps_per_mm = Y_AXIS_MICRO_STEPS_PER_MM,
      .max_distance_in_mm = Y_AXIS_MAX_DISTANCE_IN_MM,
      .done_bit = EVENT_Y_DONE,
  };

  k_event_init(&gantry_events);
  stepper_set_done_event(gantry.x.stepper, &gantry_events, gantry.x.done_bit);
  stepper_set_done_event(gantry.y.stepper, &gantry_events, gantry.y.done_bit);

  return 0;
}

enum gantry_result gantry_move(enum gantry_pos pos) {
  if (atomic_get(&gantry.busy)) {
    return GANTRY_ERR_BUSY;
  }

  atomic_set(&gantry.busy, 1);
  enum gantry_result res;
  res = pos == POS_HOME ? gantry_move_to_home() : gantry_move_to_cubby(pos);
  atomic_clear(&gantry.busy);
  return res;
}

void gantry_halt(void) {
  stepper_stop(gantry.x.stepper);
  stepper_stop(gantry.y.stepper);
  k_event_post(&gantry_events, EVENT_ABORT);
  return;
}
