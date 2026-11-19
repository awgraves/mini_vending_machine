#include <lvgl.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 5

struct button {
  const char *name;
  lv_align_t align;
  int32_t x_offset;
  int32_t y_offset;
  uint32_t cnt;
};

#define BUTTON(NAME, ALIGN, X, Y)                                              \
  {.name = NAME, .align = ALIGN, .x_offset = X, .y_offset = Y, .cnt = 0}

static struct button buttons[] = {
    BUTTON("Top", LV_ALIGN_TOP_MID, 0, 10),
    BUTTON("Left", LV_ALIGN_LEFT_MID, 10, 0),
    BUTTON("Middle", LV_ALIGN_CENTER, 0, 0),
    BUTTON("Right", LV_ALIGN_RIGHT_MID, -10, 0),
    BUTTON("Bottom", LV_ALIGN_BOTTOM_MID, 0, -10)};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(struct button))

static const struct device *display_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

void btn_handler(lv_event_t *e) {
  struct button *b = (struct button *)lv_event_get_user_data(e);

  b->cnt++; // reminder to self, not thread safe!

  lv_obj_t *obj = lv_event_get_target_obj(e);
  lv_obj_t *label = lv_obj_get_child(obj, 0);
  lv_label_set_text_fmt(label, "%s: %d", b->name, b->cnt);
}

int main(void) {
  if (!device_is_ready(display_dev)) {
    printk("Display not ready\n");
    return 0;
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    struct button *b = &buttons[i];
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_align(btn, b->align, b->x_offset, b->y_offset);
    lv_obj_add_event_cb(btn, btn_handler, LV_EVENT_CLICKED, b);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text_fmt(label, "%s: %d", b->name, b->cnt);
    lv_obj_center(label);
  }

  display_blanking_off(display_dev);

  while (1) {
    lv_timer_handler();

    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
}
