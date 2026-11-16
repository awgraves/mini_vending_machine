#include <zephyr/drivers/led.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>

#define BLINK_DELAY_MS 1000
#define SAMPLE_LED_BRIGHTNESS 75 // between 0 and 255

#define RGB(_r, _g, _b) {.r = (_r), .g = (_g), .b = (_b)}

static struct led_rgb colors[] = {
    RGB(SAMPLE_LED_BRIGHTNESS, 0x00, 0x00), /* red */
    RGB(0x00, SAMPLE_LED_BRIGHTNESS, 0x00), /* green */
    RGB(0x00, 0x00, SAMPLE_LED_BRIGHTNESS), /* blue */
};

static const struct device *strip = DEVICE_DT_GET(DT_ALIAS(ledstrip));

const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led0));

int main(void) {
  if (!led_is_ready_dt(&led)) {
    return -ENODEV;
  }

  if (!device_is_ready(strip)) {
    return -ENODEV;
  }

  int ext_led_phase = 0;

  while (1) {
    for (int i = 0; i < 3; i++) {
      led_strip_update_rgb(strip, &colors[i], 1);
      ext_led_phase ? led_off_dt(&led) : led_on_dt(&led);
      ext_led_phase = (ext_led_phase + 1) & 0x1;

      k_msleep(BLINK_DELAY_MS);
    }
  }

  return 0;
}
