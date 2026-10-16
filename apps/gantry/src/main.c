#include "gantry.h"
#include <stdio.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20

void loop_message(const char *msg) {
  while (1) {
    printf("%s\n", msg);
    k_msleep(SLEEP_TIME_MS);
  }
}

uint8_t cmd_idx;

int main(void) {
  gantry_init();

  k_msleep(1000);

  enum gantry_pos positions[7] = {POS_A, POS_B, POS_C,   POS_D,
                                  POS_E, POS_F, POS_HOME};
  enum gantry_result res;
  for (int i = 0; i < (sizeof(positions) / sizeof(enum gantry_pos)); i++) {
    printf("Trying position %d...\n", positions[i]);
    res = gantry_move(positions[i]);
    if (res != GANTRY_OK) {
      printf("gantry cmd failed!: Err code: %d\n", res);
    }

    k_msleep(1000);
  }

  while (1) {
    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
};
