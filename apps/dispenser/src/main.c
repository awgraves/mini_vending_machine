#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 1000

const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));
const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw), gpios);
const struct device *can_dev = DEVICE_DT_GET(DT_ALIAS(canbus));

static K_SEM_DEFINE(tx_done, 0, 1);

static void tx_cb(const struct device *dev, int error, void *user_data) {
  printk("tx callback, error=%d\n", error);
  k_sem_give(&tx_done);
}

CAN_MSGQ_DEFINE(rx_q, 4);

const struct can_filter filter = {
    .id = 0x123, .mask = CAN_STD_ID_MASK, .flags = 0};

int main(void) {

  k_msleep(5000);
  printk("Starting up...\n");

  if (!led_is_ready_dt(&led) || !gpio_is_ready_dt(&btn) ||
      !device_is_ready(can_dev)) {
    printk("A device failed to init\n");
    return 0;
  }

  gpio_pin_configure_dt(&btn, (GPIO_ACTIVE_LOW | GPIO_PULL_UP | GPIO_INPUT));

  int ret;

  ret = can_set_bitrate(can_dev, 250000); // 250kb/s
  if (ret) {
    printk("set_bitrate failed [%d]\n", ret);
    return 0;
  }
  can_set_mode(can_dev, CAN_MODE_LOOPBACK); // needed if only module on bus
  ret = can_start(can_dev);
  if (ret) {
    printk("can_start failed [%d]\n", ret);
    return 0;
  }

  printk("CAN should be started\n");

  can_add_rx_filter_msgq(can_dev, &rx_q, &filter);

  struct can_frame tx = {
      .flags = 0, /* 0 = standard 11-bit ID */
      .id = 0x123,
      .dlc = 8,
      .data = {1, 2, 3, 4, 5, 6, 7, 8},
  };
  struct can_frame rx;

  printk("made it, should start looping\n");

  while (1) {
    if (gpio_pin_get_dt(&btn)) {
      led_on_dt(&led);
    } else {
      led_off_dt(&led);
    }
    ret = can_send(can_dev, &tx, K_MSEC(100), tx_cb, NULL);
    printk("can_send queued: %d\n", ret);

    if (k_sem_take(&tx_done, K_MSEC(500)) != 0) {
      enum can_state state;
      struct can_bus_err_cnt cnt;
      can_get_state(can_dev, &state, &cnt);
      printk("TX never completed. state=%d tx_err=%u rx_err=%u\n", state,
             cnt.tx_err_cnt, cnt.rx_err_cnt);
    }

    if (k_msgq_get(&rx_q, &rx, K_MSEC(200)) == 0) {
      printk("RECEIVED! rx id=0x%x dlc=%u data=", rx.id, rx.dlc);
      for (int i = 0; i < rx.dlc; i++) {
        printk("%u ", rx.data[i]);
      }
      printk("\n");
    }
    k_msleep(DELAY_MS);
  }
  return 0;
}
