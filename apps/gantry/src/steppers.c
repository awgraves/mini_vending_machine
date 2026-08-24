#include "steppers.h"
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

static struct stepper_run_t x_state = {0};

static const struct device *stepper_driver =
    DEVICE_DT_GET(DT_ALIAS(stepper_driver));
static const struct device *stepper_ctrl =
    DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));

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
  (IHOLD_IRUN_IHOLD(4) | IHOLD_IRUN_IRUN(16) | IHOLD_IRUN_IHOLDDELAY(4))

#define MIN_NS_INTERVAL 200000
#define NS_INTERVAL_PER_TICK (MIN_NS_INTERVAL / 100)
#define NS_INTERVAL_SLOWEST (MIN_NS_INTERVAL + (NS_INTERVAL_PER_TICK * 100))

static inline uint64_t get_microstep_interval(uint8_t speed) {
  return NS_INTERVAL_SLOWEST - (NS_INTERVAL_PER_TICK * speed);
};

static const struct device *const uart_dev =
    DEVICE_DT_GET(DT_ALIAS(stepper_uart));

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

/*
  Public API
*/

int steppers_init(void) {
  int ret;
  if (!device_is_ready(stepper_ctrl) || !device_is_ready(uart_dev)) {
    return -ENODEV;
  }

  // uart config
  write(GCONF_REG_ADDR, GCONF_VALS);
  write(IHOLD_IRUN_REG_ADDR, IHOLD_IRUN_VALS);
  write(CHOPCONF_REG_ADDR, CHOPCONF_VALS);

  if ((ret = stepper_enable(stepper_driver)) < 0) {
    return ret;
  }

  return 0;
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
