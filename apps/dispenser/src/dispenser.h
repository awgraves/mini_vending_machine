#pragma once

enum cubby_id {
  CUBBY_A,
  CUBBY_B,
  CUBBY_C,
  CUBBY_D,
  CUBBY_E,
  CUBBY_F,
  NUM_CUBBIES
};

enum dispenser_result {
  DISPENSER_OK,
  DISPENSER_ERR_BUSY,
  DISPENSER_ERR_TIMEOUT,
};

int dispenser_init(void);

// dispense is a blocking call until item is dispensed,
// or timeout occurs.
enum dispenser_result dispense(enum cubby_id id);
