#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 1
#define WAIT_MAX_MS 5000

#define LED_NODE DT_ALIAS(led)
#define BEAM_NODE DT_ALIAS(beam0)
#define BUTTON_NODE DT_ALIAS(sw0)
#define NUM_MOTORS 3

static const struct led_dt_spec led = LED_DT_SPEC_GET(LED_NODE);
static const struct gpio_dt_spec motor_a =
    GPIO_DT_SPEC_GET(DT_ALIAS(motor0), gpios);
static const struct gpio_dt_spec motor_b =
    GPIO_DT_SPEC_GET(DT_ALIAS(motor1), gpios);
static const struct gpio_dt_spec motor_c =
    GPIO_DT_SPEC_GET(DT_ALIAS(motor2), gpios);

static const struct gpio_dt_spec beam = GPIO_DT_SPEC_GET(BEAM_NODE, gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static const struct gpio_dt_spec *motors[NUM_MOTORS] = {&motor_a, &motor_b,
                                                        &motor_c};

enum phase {
  MOVE_UNTIL_BAR_BREAKS_BEAM,
  MOVE_UNTIL_BAR_CLEARS_BEAM,
  STOP,
};

static const bool next_phase_beam_cond[2] = {
    [MOVE_UNTIL_BAR_BREAKS_BEAM] = true,
    [MOVE_UNTIL_BAR_CLEARS_BEAM] = false,
};

static enum phase curr_phase;

void poll_beam(int *reading) {
  *reading = gpio_pin_get_dt(&beam);
  *reading ? led_on_dt(&led) : led_off_dt(&led);
}

// returns true if motor should continue for next phase
// false if end of cycle
bool move_until_next_phase(void) {
  if (curr_phase >= STOP) {
    return false;
  }

  bool cond_to_proceed = next_phase_beam_cond[curr_phase];

  int reading;
  int wait_count = WAIT_MAX_MS;
  while (1) {
    poll_beam(&reading);
    if (reading == cond_to_proceed) {
      break;
    }

    wait_count--;
    if (wait_count <= 0) {
      return false;
    }
    k_msleep(DELAY_MS);
  }

  curr_phase++;
  return true;
}

int main(void) {
  curr_phase = STOP;

  // DT ready
  if (!led_is_ready_dt(&led) || !gpio_is_ready_dt(&beam) ||
      !gpio_is_ready_dt(&btn)) {
    return 0;
  }

  for (int i = 0; i < NUM_MOTORS; i++) {
    if (!gpio_is_ready_dt(motors[i])) {
      return 0;
    }
  }

  gpio_pin_configure_dt(&beam, GPIO_INPUT);
  gpio_pin_configure_dt(&btn, GPIO_INPUT);

  // int beam_broken;

  while (1) {
    for (int i = 0; i < NUM_MOTORS; i++) {
      const struct gpio_dt_spec *motor = motors[i];

      while (!gpio_pin_get_dt(&btn)) {
        k_msleep(DELAY_MS);
      }

      // poll_beam(&beam_broken);
      // curr_phase =
      //     beam_broken ? MOVE_UNTIL_BAR_CLEARS_BEAM :
      //     MOVE_UNTIL_BAR_BREAKS_BEAM;

      led_on_dt(&led);
      gpio_pin_set_dt(motor, 1);
      k_msleep(2000);
      // while (move_until_next_phase())
      //   ;
      gpio_pin_set_dt(motor, 0);
      led_off_dt(&led);
      // refresh led state before awaiting next loop
      // poll_beam(&beam_broken);
    }
  }

  return 0;
}
