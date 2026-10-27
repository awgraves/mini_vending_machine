#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 2
#define LED_NODE DT_ALIAS(led)

static const struct led_dt_spec led = LED_DT_SPEC_GET(LED_NODE);
static const struct gpio_dt_spec beam =
    GPIO_DT_SPEC_GET(DT_ALIAS(beam0), gpios);

static bool prev;

int main(void) {

  if (!led_is_ready_dt(&led)) {
    return 0;
  }

  if (!gpio_is_ready_dt(&beam)) {
    return 0;
  }

  gpio_pin_configure_dt(&beam, GPIO_INPUT);

  while (1) {
    bool new = gpio_pin_get_dt(&beam);
    if (new != prev) {
      new ? led_on_dt(&led) : led_off_dt(&led);
      prev = new;
    }
    k_msleep(DELAY_MS);
  }

  return 0;
}
