#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 1
#define WAIT_MAX_MS 5000

#define BUTTON_NODE DT_ALIAS(sw0)

struct cubby {
  const struct gpio_dt_spec motor;
  const struct gpio_dt_spec beam;
};

#define CUBBY(motor_alias, beam_alias)                                         \
  {.motor = GPIO_DT_SPEC_GET(DT_ALIAS(motor_alias), gpios),                    \
   .beam = GPIO_DT_SPEC_GET(DT_ALIAS(beam_alias), gpios)}

#define NUM_CUBBIES 6
static const struct cubby cubbies[NUM_CUBBIES] = {
    CUBBY(motora, beama), CUBBY(motorb, beamb), CUBBY(motorc, beamc),
    CUBBY(motord, beamd), CUBBY(motore, beame), CUBBY(motorf, beamf)};

static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

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

void poll_beam(const struct gpio_dt_spec *beam, int *reading) {
  *reading = gpio_pin_get_dt(beam);
  *reading ? led_on_dt(&led) : led_off_dt(&led);
}

// returns true if motor should continue for next phase
// false if end of cycle
bool move_until_next_phase(const struct gpio_dt_spec *beam) {
  if (curr_phase >= STOP) {
    return false;
  }

  bool cond_to_proceed = next_phase_beam_cond[curr_phase];

  int reading;
  int wait_count = WAIT_MAX_MS;
  while (1) {
    poll_beam(beam, &reading);
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

  if (!led_is_ready_dt(&led) || !gpio_is_ready_dt(&btn)) {
    return 0;
  }
  gpio_pin_configure_dt(&btn, GPIO_INPUT);

  for (int i = 0; i < NUM_CUBBIES; i++) {
    const struct cubby *c = &cubbies[i];

    if (!gpio_is_ready_dt(&c->motor)) {
      return 0;
    }
    gpio_pin_configure_dt(&c->motor, GPIO_OUTPUT_INACTIVE);

    if (!gpio_is_ready_dt(&c->beam)) {
      return 0;
    }
    gpio_pin_configure_dt(&c->beam, GPIO_INPUT);
  }

  int beam_broken;

  const struct cubby *c;
  while (1) {
    for (int i = 0; i < NUM_CUBBIES; i++) {
      c = &cubbies[i];
      while (!gpio_pin_get_dt(&btn)) {
        k_msleep(DELAY_MS);
      }

      poll_beam(&c->beam, &beam_broken);
      curr_phase =
          beam_broken ? MOVE_UNTIL_BAR_CLEARS_BEAM : MOVE_UNTIL_BAR_BREAKS_BEAM;

      gpio_pin_set_dt(&c->motor, 1);
      while (move_until_next_phase(&c->beam))
        ;
      gpio_pin_set_dt(&c->motor, 0);
      // refresh led state before awaiting next loop
      poll_beam(&c->beam, &beam_broken);
    }
  }

  return 0;
}
