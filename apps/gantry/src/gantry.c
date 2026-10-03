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
#define X_AXIS_MAX_DISTANCE_IN_MM 310

#define HOMING_SPEED_Y 20
#define MAX_SPEED_Y 50
#define HOMING_SPEED_X 10
#define MAX_SPEED_X 50

struct coords_in_mm {
  uint32_t x;
  uint32_t y;
};

#define COL_1 310
#define COL_2 220
#define COL_3 130
#define TOP_ROW 102
#define BOTTOM_ROW 0

static const struct coords_in_mm positions[NUM_GANTRY_POSITIONS] = {
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

struct gantry {
  bool calibrated;
  struct axis x;
  struct axis y;
  gantry_event_cb_t event_cb;
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

// ----------------
// ---- Events ----
// ----------------

static void _pub_event(struct gantry_event *ev) {
  if (gantry.event_cb) {
    gantry.event_cb(ev);
  }
}

static void pub_err(enum gantry_err_type err) {
  struct gantry_event ev = {
      .type = GANTRY_ERR,
      .data = (void *)err,
  };
  _pub_event(&ev);
}

static void pub_move_completed(enum gantry_pos pos) {
  struct gantry_event ev = {
      .type = GANTRY_MOVE_COMPLETED,
      .data = (void *)pos,
  };
  _pub_event(&ev);
}

// --------------------------
// ---- Command handlers ----
// --------------------------

static bool axis_calibration(struct axis *a) {
  /*
    If the switch is already engaged, back things up slightly,
    then assert it is no longer engaged before proceeding.
    This adds extra assurance that an engaged switch is an accurate reading.
  */
  uint8_t speed = (a == &gantry.x) ? HOMING_SPEED_X : HOMING_SPEED_Y;
  if (stepper_get_is_at_limit(a->stepper)) {
    stepper_set_speed(a->stepper, speed);
    stepper_move_steps(a->stepper, millimeters_to_steps(a, 10));
    k_sem_take(&a->event_sem, K_SECONDS(2));
    k_msleep(50);
    if (stepper_get_is_at_limit(a->stepper)) {
      return false;
    }
  }

  stepper_run_until_limit_hit(a->stepper, speed);
  k_sem_take(&a->event_sem, K_SECONDS(10));
  if (!stepper_get_is_at_limit(a->stepper)) {
    return false;
  }

  return true;
}

enum gantry_home_mode {
  GANTRY_HOME_SILENT_CALIBRATION,
  GANTRY_HOME_EMIT_COMPLETION_EVENT
};

static bool gantry_home(enum gantry_home_mode mode) {
  // special logic for home moves, also covers calibration
  if (axis_calibration(&gantry.y) && axis_calibration(&gantry.x)) {
    gantry.calibrated = true;
    if (mode == GANTRY_HOME_EMIT_COMPLETION_EVENT) {
      pub_move_completed(POS_HOME);
    }
    return true;
  }

  gantry.calibrated = false;
  pub_err(ERR_HOMING_FAILURE);
  return false;
}

static inline void axis_move_to_mm_pos(struct axis *a, int target_mm) {
  int32_t move_delta = target_mm - get_curr_axis_pos_in_mm(a);
  printf("Target mm pos %d for %c\n", target_mm, a == &gantry.x ? 'x' : 'y');
  uint8_t speed = (a == &gantry.x) ? MAX_SPEED_X : MAX_SPEED_Y;

  if (target_mm == 0) {
    stepper_run_until_limit_hit(a->stepper, speed);
    printf("Run until limit hit...\n");
  } else {
    printf("moving %d steps...\n", move_delta);
    stepper_set_speed(a->stepper, speed);
    stepper_move_steps(a->stepper, millimeters_to_steps(a, move_delta));
  }
}

static void gantry_move_to_pos(enum gantry_pos pos) {
  // if gantry requires calibration, must do homing first
  if (!gantry.calibrated && pos != POS_HOME) {
    if (!gantry_home(GANTRY_HOME_SILENT_CALIBRATION)) {
      return; // prevent actual move if homing failed
    }
    k_msleep(500); // small delay before next move
  }
  const struct coords_in_mm *target = get_coords_for_position(pos);
  if (target->x > gantry.x.max_distance_in_mm ||
      target->y > gantry.y.max_distance_in_mm) {
    pub_err(ERR_COORDINATES_OUT_OF_BOUNDS);
    return;
  }

  axis_move_to_mm_pos(&gantry.x, target->x);
  axis_move_to_mm_pos(&gantry.y, target->y);

  k_sem_take(&gantry.x.event_sem, K_FOREVER);
  k_sem_take(&gantry.y.event_sem, K_FOREVER);

  pub_move_completed(pos);
  return;
}

// --------------------
// ---- CMD Thread ----
// --------------------

// max 2 messages, 32 bit alignment (4 bytes)
K_MSGQ_DEFINE(gantry_cmdq, sizeof(struct gantry_cmd), 2, 4);

K_THREAD_STACK_DEFINE(gantry_stack, 1024);
static struct k_thread gantry_thread_data;

static void gantry_thread_fn(void *a, void *b, void *c) {
  ARG_UNUSED(a);
  ARG_UNUSED(b);
  ARG_UNUSED(c);

  struct gantry_cmd cmd;

  while (1) {
    k_msgq_get(&gantry_cmdq, &cmd, K_FOREVER);

    switch (cmd.type) {
    case GANTRY_MOVE:
      gantry_move_to_pos(cmd.target_pos);
      // todo
      break;
    case GANTRY_HALT:
      // TODO, this should be handled outside the thread
      break;
    }
  }
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

  gantry.calibrated = false;
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

  k_thread_create(&gantry_thread_data, gantry_stack,
                  K_THREAD_STACK_SIZEOF(gantry_stack), gantry_thread_fn, NULL,
                  NULL, NULL, 5, 0, K_NO_WAIT);

  return 0;
}

void gantry_register_event_cb(gantry_event_cb_t cb) { gantry.event_cb = cb; }

int gantry_cmd_send(struct gantry_cmd *cmd) {
  // TODO let halt command skip the queue
  int ret = k_msgq_put(&gantry_cmdq, cmd, K_NO_WAIT);
  if (ret != 0) {
    return ret;
  }

  return 0;
}
