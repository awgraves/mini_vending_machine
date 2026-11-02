#include "gantry.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define SLEEP_TIME_MS 20
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led0));

uint8_t cmd_idx;

static void move(enum gantry_pos pos) {
  led_on_dt(&led);
  gantry_move(pos);
  led_off_dt(&led);
}

int main(void) {
  gantry_init();

  enum gantry_pos positions[6] = {POS_A, POS_B, POS_C, POS_D, POS_E, POS_F};

  if (!gpio_is_ready_dt(&btn)) {
    led_on_dt(&led);
    return -1;
  }

  gpio_pin_configure_dt(&btn, GPIO_INPUT);

  while (1) {
    for (int i = 0; i < 6; i++) {
      while (!gpio_pin_get_dt(&btn)) {
        k_msleep(SLEEP_TIME_MS);
      }
      move(positions[i]);

      while (!gpio_pin_get_dt(&btn)) {
        k_msleep(SLEEP_TIME_MS);
      }
      move(POS_HOME);
    }
  }

  return 0;
};
