#pragma once

enum gantry_pos {
  POS_HOME,
  POS_A,
  POS_B,
  POS_C,
  POS_D,
  POS_E,
  POS_F,
  NUM_GANTRY_POSITIONS
};

enum gantry_cmd_type {
  GANTRY_MOVE,
  GANTRY_HALT,
};

struct gantry_cmd {
  enum gantry_cmd_type type;
  enum gantry_pos target_pos; // expected for GANTRY_MOVE cmds
};

enum gantry_event_type {
  GANTRY_ERR,
  GANTRY_MOVE_COMPLETED,
  GANTRY_MOVE_HALTED,
};

enum gantry_err_type {
  ERR_MOVE_BEFORE_CALIBRATED,
  ERR_COORDINATES_OUT_OF_BOUNDS,
  ERR_HOMING_FAILURE,
};

struct gantry_event {
  enum gantry_event_type type;
  void *data; // depends on event type
};

// make sure this func is short and non-blocking
typedef void (*gantry_event_cb_t) (const struct gantry_event *ev);

int gantry_init(void);
void gantry_register_event_cb(gantry_event_cb_t cb);

// 0 ret = successfully sent, might be -int if queue is full
int gantry_cmd_send(struct gantry_cmd *cmd);
