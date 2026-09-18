#include "steppers.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

struct stepper {
  const char name;
  const struct device *driver;
  const struct device *ctrl;
  const struct gpio_dt_spec limit_sw;
  enum stepper_ctrl_direction dir_of_limit_sw;
  limit_hit_callback_t limit_hit_cb;
};

struct steppers {
  struct stepper x;
  struct stepper y;
};

static struct steppers steppers = {
    .x =
        {
            .name = 'X',
            .driver = DEVICE_DT_GET(DT_ALIAS(stepper_driver_x)),
            .ctrl = DEVICE_DT_GET(DT_ALIAS(stepper_ctrl_x)),
            .limit_sw = GPIO_DT_SPEC_GET(DT_ALIAS(stepper_limit_x), gpios),
            .dir_of_limit_sw = STEPPER_CTRL_DIRECTION_POSITIVE,
            .limit_hit_cb = NULL,
        },
    .y = {
        .name = 'Y',
        .driver = DEVICE_DT_GET(DT_ALIAS(stepper_driver_y)),
        .ctrl = DEVICE_DT_GET(DT_ALIAS(stepper_ctrl_y)),
        .limit_sw = GPIO_DT_SPEC_GET(DT_ALIAS(stepper_limit_y), gpios),
        .dir_of_limit_sw = STEPPER_CTRL_DIRECTION_NEGATIVE,
        .limit_hit_cb = NULL,
    }};

/*
  Helpers
*/

// per
// https://www.analog.com/media/en/technical-documentation/data-sheets/tmc2209_datasheet_rev1.09.pdf
struct datagram {
  uint8_t sync;
  uint8_t node_addr; // 0 - 3
  uint8_t reg;       // highest bit is the read/or write, 7 remaining are addr
  uint8_t data[4];   // big endian
  uint8_t crc;
};

struct datagram_read {
  uint8_t sync;
  uint8_t node_addr;
  uint8_t reg;
  uint8_t crc;
};

#define TMC2209_SYNC 0x05
#define TMC2209_BUS_ADDR 0x0

// per pg. 20 of tmc2209 datasheet
void add_crc(uint8_t *datagram, uint8_t len) {
  int i, j;
  uint8_t *crc = datagram + (len - 1); // CRC located in last byte of message
  uint8_t currentByte;
  *crc = 0;
  for (i = 0; i < (len - 1); i++) { // Execute for all bytes of a message
    currentByte = datagram[i];      // Retrieve a byte to be sent from Array
    for (j = 0; j < 8; j++) {
      if ((*crc >> 7) ^
          (currentByte & 0x01)) // update CRC based result of XOR operation
      {
        *crc = (*crc << 1) ^ 0x07;
      } else {
        *crc = (*crc << 1);
      }
      currentByte = currentByte >> 1;
    } // for CRC bit
  } // for message byte
}

static void build_write_datagram(struct datagram *dg, uint8_t dev_addr,
                                 uint8_t reg, uint32_t value) {
  dg->sync = TMC2209_SYNC;
  dg->node_addr = dev_addr;
  dg->reg = reg | 0x80;      // always write mode
  dg->data[0] = value >> 24; // swap to big endian
  dg->data[1] = value >> 16;
  dg->data[2] = value >> 8;
  dg->data[3] = value >> 0;
  add_crc((uint8_t *)dg, sizeof(struct datagram));
}

static void build_read_datagram(struct datagram_read *dg, uint8_t dev_addr,
                                uint8_t reg) {
  dg->sync = TMC2209_SYNC;
  dg->node_addr = dev_addr;
  dg->reg = reg;
  add_crc((uint8_t *)dg, sizeof(struct datagram_read));
}

// pg. 23
#define GCONF_REG_ADDR 0x00
#define GCONF_PDN_DISABLE (0x1 << 6)
#define GCONF_MSTEP_REG_SELECT_UART (0x1 << 7) // rather than ms1/ms2 pins
#define GCONF_VALS (GCONF_PDN_DISABLE | GCONF_MSTEP_REG_SELECT_UART)

#define IFCNT 0x02

#define CHOPCONF_REG_ADDR 0x6C
#define CHOPCONF_MRES(s) (((s) & 0xF) << 24)
#define MRES_8 0x5
#define MRES_16 0x4

// taking these defaults from pg. 70
#define CHOPCONF_TOFF (0x5)
#define CHOPCONF_TBL (0x2 << 15)
#define CHOPCONF_HSTART (0x4 << 4)
#define CHOPCONF_HEND (0 << 7)
#define CHOPCONF_VALS                                                          \
  (CHOPCONF_TOFF | CHOPCONF_TBL | CHOPCONF_HSTART | CHOPCONF_HEND |            \
   CHOPCONF_MRES(MRES_8))

// pg. 28
#define IHOLD_IRUN_REG_ADDR 0x10
#define IHOLD_IRUN_IHOLD(v) ((v) & 0x1F)
#define IHOLD_IRUN_IRUN(v) (((v) & 0x1F) << 8)
#define IHOLD_IRUN_IHOLDDELAY(v) (((v) & 0xF) << 16)
// targeting max 1A, formula on pg. 53
#define IHOLD_IRUN_VALS                                                        \
  (IHOLD_IRUN_IHOLD(2) | IHOLD_IRUN_IRUN(16) | IHOLD_IRUN_IHOLDDELAY(4))

#define MIN_NS_INTERVAL 100000
#define NS_INTERVAL_PER_TICK (MIN_NS_INTERVAL / 100)
#define NS_INTERVAL_SLOWEST (MIN_NS_INTERVAL + (NS_INTERVAL_PER_TICK * 100))

static inline uint64_t get_microstep_interval(uint8_t speed) {
  return NS_INTERVAL_SLOWEST - (NS_INTERVAL_PER_TICK * speed);
};

static const struct device *const uart_dev =
    DEVICE_DT_GET(DT_ALIAS(steppers_uart));

// used just for debugging with logic analyzer
void read(uint8_t reg) {
  struct datagram_read dgr;
  uint8_t *dgr_bytes = (uint8_t *)&dgr;

  build_read_datagram(&dgr, TMC2209_BUS_ADDR, reg);
  for (int i = 0; i < sizeof(struct datagram_read); i++) {
    uart_poll_out(uart_dev, dgr_bytes[i]);
  }
  k_msleep(1);
}

void write(uint8_t reg, uint32_t value) {
  struct datagram dg;
  build_write_datagram(&dg, TMC2209_BUS_ADDR, reg, value);

  uint8_t *dg_bytes = (uint8_t *)&dg;
  for (int i = 0; i < sizeof(struct datagram); i++) {
    uart_poll_out(uart_dev, dg_bytes[i]);
  }
}

void log_err(const struct stepper *stepper, const char *msg) {
  printk("Error - Stepper %c: %s\n", stepper->name, msg);
}

// ---- Limit Switches -----
static struct gpio_callback limit_sw_cb_data;

void limit_switch_isr(const struct device *dev, struct gpio_callback *cb,
                      uint32_t pins) {
  struct stepper *hits[2] = {0};
  uint8_t idx = 0;

  if (BIT(steppers.x.limit_sw.pin) & pins) {
    hits[idx++] = &steppers.x;
  };
  if (BIT(steppers.y.limit_sw.pin) & pins) {
    hits[idx++] = &steppers.y;
  }

  for (int i = 0; i < idx; i++) {
    stepper_stop(hits[i]);
    if (hits[i]->limit_hit_cb) {
      hits[i]->limit_hit_cb();
    }
  }
}

static inline bool is_valid_speed(uint8_t speed) {
  return (speed > 0 && speed <= 100);
}

/*
  Public API
*/

static K_SEM_DEFINE(stepper_sem, 0, 1);

// TODO: use this once moving set step amounts
//
// static void stepper_callback(const struct device *dev,
//                             const enum stepper_ctrl_event event,
//                             void *user_data) {
//  int32_t pos;
//  switch (event) {
//  case STEPPER_CTRL_EVENT_STEPS_COMPLETED:
//    stepper_ctrl_get_actual_position(dev, &pos);
//    printf("stepper thinks its at: %d\n", pos);
//    k_msleep(1000);
//    k_sem_give(&stepper_sem);
//    break;
//  default:
//    break;
//  }
//}

int steppers_init(struct steppers_config *conf) {
  int ret;
  int limit_switches_pin_mask = 0;

  k_msleep(2000);

  struct stepper *stepper = &steppers.x;
  for (int i = 0; i < 2; i++) {
    printf("Stepper %c\n", stepper->name);
    // Setup driver and controller
    if (!device_is_ready(stepper->driver)) {
      log_err(stepper, "failed to init driver");
      return -ENODEV;
    }

    if ((ret = stepper_enable(stepper->ctrl)) < 0) {
      log_err(stepper, "failed to init controller");
      return ret;
    }

    // Setup limit switch
    if (!gpio_is_ready_dt(&stepper->limit_sw)) {
      log_err(stepper, "failed to init limit switch");
      return -ENODEV;
    }
    stepper->limit_hit_cb = stepper == &steppers.x
                                ? conf->callbacks.limit_hit_x
                                : conf->callbacks.limit_hit_y;

    if ((ret = gpio_pin_configure_dt(&stepper->limit_sw, GPIO_INPUT)) < 0) {
      log_err(stepper, "failed to configure limit as input");
      return ret;
    }

    if ((ret = gpio_pin_interrupt_configure_dt(&stepper->limit_sw,
                                               GPIO_INT_EDGE_TO_ACTIVE)) < 0) {
      log_err(stepper, "faled to configure limit interrupt");
      return ret;
    }
    limit_switches_pin_mask |= BIT(stepper->limit_sw.pin);
    gpio_add_callback(stepper->limit_sw.port, &limit_sw_cb_data);

    stepper++;
  }

  // shared limit switch ISR init
  printf("adding limit switch isr\n");
  gpio_init_callback(&limit_sw_cb_data, limit_switch_isr,
                     limit_switches_pin_mask);

  printf("prior to uart\n");
  // uart config
  if (!device_is_ready(uart_dev)) {
    return -ENODEV;
  }
  printf("made it uart \n");

  // both steppers share the same bus address 0, same config for both
  write(GCONF_REG_ADDR, GCONF_VALS);
  write(IHOLD_IRUN_REG_ADDR, IHOLD_IRUN_VALS);
  write(CHOPCONF_REG_ADDR, CHOPCONF_VALS);

  printf("made it post write\n");
  // k_msleep(2000);
  printf("made it here\n");
  stepper_run_until_limit_hit(&steppers.x, 20);
  stepper_run_until_limit_hit(&steppers.y, 20);

  // test positioning
  // struct stepper test_stepper = steppers[0];
  // stepper_ctrl_set_reference_position(test_stepper.ctrl, 0);
  // stepper_ctrl_set_microstep_interval(test_stepper.ctrl, 100000);

  // stepper_ctrl_set_event_cb(test_stepper.ctrl, stepper_callback, NULL);

  // stepper_ctrl_move_by(test_stepper.ctrl, MICRO_STEPS_PER_REV);

  // k_sem_take(&stepper_sem, K_FOREVER);
  // stepper_ctrl_move_by(test_stepper.ctrl, -MICRO_STEPS_PER_REV);

  return 0;
};

struct stepper_handles get_stepper_handles(void) {
  return (struct stepper_handles){.x = &steppers.x, .y = &steppers.y};
}

int stepper_stop(struct stepper *s) { return stepper_ctrl_stop(s->ctrl); }
int stepper_run(struct stepper *s, const struct stepper_run_conf *conf) {
  if (!is_valid_speed(conf->speed)) {
    return -EINVAL;
  }

  if (conf->dir == s->dir_of_limit_sw && stepper_get_is_at_limit(s)) {
    // prevent movement
    return 0;
  }

  int ret;
  uint64_t ns_interval = get_microstep_interval(conf->speed);
  ret = stepper_ctrl_set_microstep_interval(s->ctrl, ns_interval);
  if (ret < 0) {
    return ret;
  }

  ret = stepper_ctrl_run(s->ctrl, conf->dir);

  return 0;
}

bool stepper_get_is_at_limit(struct stepper *s) {
  return gpio_pin_get_dt(&s->limit_sw);
}

int stepper_run_until_limit_hit(struct stepper *s, uint8_t speed) {
  if (!is_valid_speed(speed)) {
    return -EINVAL;
  }

  if (stepper_get_is_at_limit(s)) {
    if (s->limit_hit_cb) {
      s->limit_hit_cb();
    }
    return 0;
  }

  uint64_t ns_interval = get_microstep_interval(speed);
  int ret = stepper_ctrl_set_microstep_interval(s->ctrl, ns_interval);
  if (ret < 0) {
    return ret;
  }
  stepper_ctrl_run(s->ctrl, s->dir_of_limit_sw);
  return 0;
}
