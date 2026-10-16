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

enum gantry_result {
  GANTRY_OK,
  GANTRY_HALTED, // when intentionally halted
  GANTRY_ERR_BUSY,
  GANTRY_ERR_SW_FAILURE,
  GANTRY_ERR_TIMEOUT,
};

int gantry_init(void);

// gantry_move is a blocking call until move finishes, errs,
// or gets halted by another thread.
enum gantry_result gantry_move(enum gantry_pos target_pos);

// gantry_halt can be called at any time from any thread.
// if a move is in progress on another thread,
// that move will end with a GANTRY_HALTED result.
void gantry_halt(void);
