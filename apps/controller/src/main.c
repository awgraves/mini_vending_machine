#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led0));

int main(void) {
  if (!led_is_ready_dt(&led)) {
    return -ENODEV;
  }

  while (1) {
    led_on_dt(&led);
    k_msleep(500);
    led_off_dt(&led);
    k_msleep(500);
  }

  return 0;
}
