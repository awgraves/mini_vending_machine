#include "can_protocol.h"
#include <lvgl.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 5

#define RX_THREAD_STACK_SIZE 1024
#define RX_THREAD_PRIORITY 2

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);

CAN_MSGQ_DEFINE(controller_can_msgq, 10);

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
  enum can_result can_res;
  char *ok_text = "ok";
  char *err_text = "ERR";
  char *res_text;

  while (1) {
    k_msgq_get(&controller_can_msgq, &frame, K_FOREVER);

    if (frame.dlc != 1U) {
      printk("Wrong data length: %u\n", frame.dlc);
      continue;
    }

    can_res = frame.data[0];
    res_text = can_res == CAN_RES_OK ? ok_text : err_text;

    printk("Result received: %s\n raw code: %u\n", res_text, can_res);
  }
}

struct button {
  const char *name;
  enum can_target target;
  lv_align_t align;
  int32_t x_offset;
  int32_t y_offset;
  uint32_t cnt;
};

#define BUTTON(NAME, TARGET, ALIGN, X, Y)                                      \
  {.name = NAME,                                                               \
   .target = TARGET,                                                           \
   .align = ALIGN,                                                             \
   .x_offset = X,                                                              \
   .y_offset = Y,                                                              \
   .cnt = 0}

static struct button buttons[] = {
    BUTTON("A", CAN_TARGET_A, LV_ALIGN_TOP_LEFT, 10, 10),
    BUTTON("B", CAN_TARGET_B, LV_ALIGN_TOP_MID, 0, 10),
    BUTTON("C", CAN_TARGET_C, LV_ALIGN_TOP_RIGHT, -10, 10),
    BUTTON("D", CAN_TARGET_D, LV_ALIGN_LEFT_MID, 10, 10),
    BUTTON("E", CAN_TARGET_E, LV_ALIGN_CENTER, 0, 10),
    BUTTON("F", CAN_TARGET_F, LV_ALIGN_RIGHT_MID, -10, 10)};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(struct button))

static const struct device *display_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

void btn_handler(lv_event_t *e) {
  struct button *b = (struct button *)lv_event_get_user_data(e);

  b->cnt++; // reminder to self, not thread safe!

  lv_obj_t *obj = lv_event_get_target_obj(e);
  lv_obj_t *label = lv_obj_get_child(obj, 0);
  lv_label_set_text_fmt(label, "%s: %u", b->name, b->cnt);

  struct can_frame frame = {
      .flags = 0,
      .id = CAN_ID_DISPENSE,
      .dlc = 1,
      .data = {b->target},
  };

  /* This sending call is non blocking. */
  can_send(can_dev, &frame, K_NO_WAIT, tx_irq_callback, "Button click");
}

int main(void) {
  if (!device_is_ready(display_dev)) {
    printk("Display not ready\n");
    return 0;
  }

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
      .id = CAN_ID_DISPENSE_RESP,
      .mask = CAN_MASK_CLASS_AND_NODE,
  };

  int filter_id =
      can_add_rx_filter_msgq(can_dev, &controller_can_msgq, &filter);
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

  for (int i = 0; i < NUM_BUTTONS; i++) {
    struct button *b = &buttons[i];
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_align(btn, b->align, b->x_offset, b->y_offset);
    lv_obj_set_size(btn, 150, 100);
    lv_obj_add_event_cb(btn, btn_handler, LV_EVENT_CLICKED, b);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%s: %d", b->name, b->cnt);
    lv_obj_center(label);
  }

  display_blanking_off(display_dev);

  printk("main loop\n");

  while (1) {
    lv_timer_handler();

    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
}
