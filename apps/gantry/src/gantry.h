#pragma once

#define NUM_GANTRY_POSITIONS 7
enum gantry_pos {
  POS_HOME,
  POS_A,
  POS_B,
  POS_C,
  POS_D,
  POS_E,
  POS_F,
};

int gantry_init(void);
int gantry_calibrate(void);

// blocking call for now
int gantry_move_to_pos(enum gantry_pos pos);
