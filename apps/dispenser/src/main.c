#include "dispenser.h"
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 1

static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

int main(void) {
  if (!gpio_is_ready_dt(&btn)) {
    return 0;
  }
  gpio_pin_configure_dt(&btn, GPIO_INPUT);

  if (dispenser_init() != 0) {
    return 0;
  };

  while (1) {
    for (int i = 0; i < NUM_CUBBIES; i++) {
      while (!gpio_pin_get_dt(&btn)) {
        k_msleep(DELAY_MS);
      }
      dispense((enum position)i);
    }
  }

  return 0;
}
