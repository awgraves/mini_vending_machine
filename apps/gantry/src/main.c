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
K_SEM_DEFINE(blocking_cmd_sem, 0, 1);

void gantry_cb(const struct gantry_event *ev) {
  if (ev->type != GANTRY_MOVE_COMPLETED) {
    printf("Something went wrong. Err code %d\n",
           (enum gantry_err_type)ev->data);
  }
  k_sem_give(&blocking_cmd_sem);
}

int main(void) {
  gantry_init();

  k_msleep(1000);

  gantry_register_event_cb(gantry_cb);

  int ret;
  struct gantry_cmd cmd;
  enum gantry_pos positions[7] = {POS_A, POS_B, POS_C,   POS_D,
                                  POS_E, POS_F, POS_HOME};
  for (cmd_idx = 0; cmd_idx < (sizeof(positions) / sizeof(enum gantry_pos));
       cmd_idx++) {
    cmd.type = GANTRY_MOVE;
    cmd.target_pos = (enum gantry_pos)positions[cmd_idx];

    ret = gantry_cmd_send(&cmd);
    if (ret < 0) {
      printf("gantry cmd failed!\n");
      break;
    }
    printf("Trying position %d...\n", positions[cmd_idx]);
    ret = k_sem_take(&blocking_cmd_sem, K_SECONDS(10));
    if (ret != 0) {
      printf("Timeout!\n");
    }

    k_msleep(1000);
  }

  while (1) {
    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
};
