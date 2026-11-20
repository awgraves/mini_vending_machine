#include <lvgl.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 5

#define RX_THREAD_STACK_SIZE 1024
#define RX_THREAD_PRIORITY 2
#define DISPENSE_MSG_ID 0x10

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);

CAN_MSGQ_DEFINE(test_msgq, 2);

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

struct k_thread rx_thread_data;

void rx_thread(void *arg1, void *arg2, void *arg3) {
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  struct can_frame frame;

  while (1) {
    k_msgq_get(&test_msgq, &frame, K_FOREVER);

    if (frame.dlc != 1U) {
      printk("Wrong data length: %u\n", frame.dlc);
      continue;
    }

    printk("Frame received.\n");

    printk("Button click received: %c\n", (char)frame.data[0]);
  }
}

struct button {
  const char *name;
  lv_align_t align;
  int32_t x_offset;
  int32_t y_offset;
  uint32_t cnt;
};

#define BUTTON(NAME, ALIGN, X, Y)                                              \
  {.name = NAME, .align = ALIGN, .x_offset = X, .y_offset = Y, .cnt = 0}

static struct button buttons[] = {BUTTON("A", LV_ALIGN_TOP_LEFT, 10, 10),
                                  BUTTON("B", LV_ALIGN_TOP_MID, 0, 10),
                                  BUTTON("C", LV_ALIGN_TOP_RIGHT, -10, 10),
                                  BUTTON("D", LV_ALIGN_LEFT_MID, 10, 10),
                                  BUTTON("E", LV_ALIGN_CENTER, 0, 10),
                                  BUTTON("F", LV_ALIGN_RIGHT_MID, -10, 10)};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(struct button))

static const struct device *display_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

void tx_irq_callback(const struct device *dev, int error, void *arg) {
  char *sender = (char *)arg;

  ARG_UNUSED(dev);

  if (error != 0) {
    printk("Callback! error-code: %d\nSender: %s\n", error, sender);
  }
}

void btn_handler(lv_event_t *e) {
  struct button *b = (struct button *)lv_event_get_user_data(e);

  b->cnt++; // reminder to self, not thread safe!

  lv_obj_t *obj = lv_event_get_target_obj(e);
  lv_obj_t *label = lv_obj_get_child(obj, 0);
  lv_label_set_text_fmt(label, "%s: %u", b->name, b->cnt);

  char c = b->name[0];
  struct can_frame frame = {
      .flags = 0,
      .id = DISPENSE_MSG_ID,
      .dlc = 1,
      .data = {c},
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

  int ret = can_set_mode(can_dev, CAN_MODE_LOOPBACK);
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
      .id = DISPENSE_MSG_ID,
      .mask = CAN_STD_ID_MASK,
  };

  int filter_id = can_add_rx_filter_msgq(can_dev, &test_msgq, &filter);
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
