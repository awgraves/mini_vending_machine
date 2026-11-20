#include "can_protocol.h"
#include "dispenser.h"
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 10

#define RX_THREAD_STACK_SIZE 1024
#define RX_THREAD_PRIORITY 2

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);

CAN_MSGQ_DEFINE(dispense_msgq, 10);

struct k_thread rx_thread_data;

void tx_irq_callback(const struct device *dev, int error, void *arg) {
  char *sender = (char *)arg;

  ARG_UNUSED(dev);

  if (error != 0) {
    printk("Callback! error-code: %d\nSender: %s\n", error, sender);
  }
}

void rx_thread(void *arg1, void *arg2, void *arg3) {
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  struct can_frame frame;
  enum can_target raw_t;

  enum cubby_id cubby;
  enum can_result can_res;
  enum dispenser_result disp_res;

  while (1) {
    k_msgq_get(&dispense_msgq, &frame, K_FOREVER);

    if (frame.dlc != 1U) {
      printk("Wrong data length: %u\n", frame.dlc);
      continue;
    }

    raw_t = frame.data[0];

    if (!is_cubby(raw_t)) {
      // TODO send bad arg response
      can_res = CAN_RES_BAD_ARG;
    } else {
      switch (raw_t) {
      case CAN_TARGET_A:
        cubby = CUBBY_A;
        break;
      case CAN_TARGET_B:
        cubby = CUBBY_B;
        break;
      case CAN_TARGET_C:
        cubby = CUBBY_C;
        break;
      case CAN_TARGET_D:
        cubby = CUBBY_D;
        break;
      case CAN_TARGET_E:
        cubby = CUBBY_E;
        break;
      case CAN_TARGET_F:
        cubby = CUBBY_F;
      default:
        // TODO send bad arg again
        break;
      }

      disp_res = dispense(cubby);

      // TODO: make this more specific
      can_res = disp_res == DISPENSER_OK ? CAN_RES_OK : CAN_RES_ERR;
    }

    struct can_frame frame = {
        .flags = 0,
        .id = CAN_ID_DISPENSE_RESP,
        .dlc = 1,
        .data = {can_res},
    };

    /* This sending call is non blocking. */
    can_send(can_dev, &frame, K_NO_WAIT, tx_irq_callback, "Dispense response");
  }
}

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

int main(void) {
  if (!gpio_is_ready_dt(&btn)) {
    return 0;
  }
  gpio_pin_configure_dt(&btn, GPIO_INPUT);

  if (dispenser_init() != 0) {
    return 0;
  };

  /* CAN setup */

  if (!device_is_ready(can_dev)) {
    printk("CAN: Device %s not ready.\n", can_dev->name);
    return 0;
  }

  int ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
  if (ret != 0) {
    printk("Error setting CAN mode [%d]\n", ret);
    return 0;
  }

  ret = can_start(can_dev);
  if (ret != 0) {
    printk("Error starting CAN controller [%d]\n", ret);
    return 0;
  }

  const struct can_filter filter = {
      .flags = 0U,
      .id = CAN_ID_DISPENSE,
      .mask = CAN_MASK_CLASS_AND_NODE,
  };

  int filter_id = can_add_rx_filter_msgq(can_dev, &dispense_msgq, &filter);
  if (filter_id < 0) {
    printk("Unable to add rx filter [%d]\n", filter_id);
    return 0;
  }

  k_tid_t rx_tid = k_thread_create(
      &rx_thread_data, rx_thread_stack, K_THREAD_STACK_SIZEOF(rx_thread_stack),
      rx_thread, NULL, NULL, NULL, RX_THREAD_PRIORITY, 0, K_NO_WAIT);
  if (!rx_tid) {
    printk("ERROR spawning rx thread\n");
  }

  while (1) {
    for (int i = 0; i < NUM_CUBBIES; i++) {
      while (!gpio_pin_get_dt(&btn)) {
        k_msleep(DELAY_MS);
      }
      dispense((enum cubby_id)i);
    }
  }

  return 0;
}
