#pragma once

enum position {
  POS_A,
  POS_B,
  POS_C,
  POS_D,
  POS_E,
  POS_F,
  NUM_CUBBIES
};

enum dispenser_result {
  DISPENSER_OK,
  DISPENSER_ERR_BUSY,
  DISPENSER_ERR_TIMEOUT
};

int dispenser_init(void);

// dispense is a blocking call until item is dispensed,
// or timeout occurs.
enum dispenser_result dispense(enum position pos);
