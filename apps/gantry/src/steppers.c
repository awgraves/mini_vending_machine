#include "steppers.h"
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/kernel.h>

static struct stepper_run_t x_state = {0};

static const struct device *stepper_driver =
    DEVICE_DT_GET(DT_ALIAS(stepper_driver));
static const struct device *stepper_ctrl =
    DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));

int steppers_init(void) {
  int ret;
  if (!device_is_ready(stepper_ctrl)) {
    return -ENODEV;
  }

  if ((ret = stepper_enable(stepper_driver)) < 0) {
    return ret;
  }

  return 0;
};

#define MIN_NS_INTERVAL 200000
#define NS_INTERVAL_PER_TICK (MIN_NS_INTERVAL / 100)
#define NS_INTERVAL_SLOWEST (MIN_NS_INTERVAL + (NS_INTERVAL_PER_TICK * 100))

static inline uint64_t get_microstep_interval(uint8_t speed) {
  return NS_INTERVAL_SLOWEST - (NS_INTERVAL_PER_TICK * speed);
};

int steppers_x_stop(void) { return stepper_ctrl_stop(stepper_ctrl); };

int steppers_x_run(struct stepper_run_t *conf) {
  if (conf->speed < 1 || conf->speed > 100) {
    return -EINVAL;
  }

  if (conf->speed == x_state.speed && conf->dir == x_state.dir) {
    // no changes
    return 0;
  }

  steppers_x_stop();

  int ret;
  uint64_t ns_interval = get_microstep_interval(conf->speed);
  ret = stepper_ctrl_set_microstep_interval(stepper_ctrl, ns_interval);
  if (ret < 0) {
    return ret;
  }

  ret = stepper_ctrl_run(stepper_ctrl, conf->dir);

  return 0;
};
