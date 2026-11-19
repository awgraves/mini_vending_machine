#include <lvgl.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20

static const struct device *display_dev =
    DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

int main(void) {
  if (!device_is_ready(display_dev)) {
    printk("Display not ready\n");
    return 0;
  }

  lv_obj_t *hello_lb = lv_label_create(lv_screen_active());
  lv_label_set_text(hello_lb, "Hello world!");
  lv_obj_align(hello_lb, LV_ALIGN_CENTER, 0, 0);

  display_blanking_off(display_dev);

  while (1) {
    lv_task_handler();

    k_msleep(SLEEP_TIME_MS);
  }

  return 0;
}
