#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 10

const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));
const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw), gpios);

int main(void) {
  if (!led_is_ready_dt(&led) || !gpio_is_ready_dt(&btn)) {
    return 0;
  }

  gpio_pin_configure_dt(&btn, (GPIO_ACTIVE_LOW | GPIO_PULL_UP | GPIO_INPUT));

  while (1) {
    if (gpio_pin_get_dt(&btn)) {
      led_on_dt(&led);
    } else {
      led_off_dt(&led);
    }
    k_msleep(DELAY_MS);
  }
  return 0;
}
