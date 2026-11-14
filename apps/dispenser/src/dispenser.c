#include "dispenser.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define WAIT_MAX_MS 5000

struct cubby {
  const struct gpio_dt_spec motor;
  const struct gpio_dt_spec beam;
};

#define CUBBY(motor_alias, beam_alias)                                         \
  {.motor = GPIO_DT_SPEC_GET(DT_ALIAS(motor_alias), gpios),                    \
   .beam = GPIO_DT_SPEC_GET(DT_ALIAS(beam_alias), gpios)}

static const struct cubby cubbies[NUM_CUBBIES] = {
    [CUBBY_A] = CUBBY(motora, beama), [CUBBY_B] = CUBBY(motorb, beamb),
    [CUBBY_C] = CUBBY(motorc, beamc), [CUBBY_D] = CUBBY(motord, beamd),
    [CUBBY_E] = CUBBY(motore, beame), [CUBBY_F] = CUBBY(motorf, beamf)};

static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));

enum phase {
  MOVE_UNTIL_BAR_BREAKS_BEAM,
  MOVE_UNTIL_BAR_CLEARS_BEAM,
  STOP,
};

static const int next_phase_beam_cond[2] = {
    [MOVE_UNTIL_BAR_BREAKS_BEAM] = 1,
    [MOVE_UNTIL_BAR_CLEARS_BEAM] = 0,
};

static atomic_t busy;

/*
  Helpers
*/

int poll_beam(const struct gpio_dt_spec *beam) {
  int res = gpio_pin_get_dt(beam);
  res ? led_on_dt(&led) : led_off_dt(&led);

  return res;
}

/*
  Public API
*/

int dispenser_init(void) {
  atomic_clear(&busy);

  if (!led_is_ready_dt(&led)) {
    return -ENODEV;
  }

  for (int i = 0; i < NUM_CUBBIES; i++) {
    const struct cubby *c = &cubbies[i];

    if (!gpio_is_ready_dt(&c->motor)) {
      return -ENODEV;
    }
    gpio_pin_configure_dt(&c->motor, GPIO_OUTPUT_INACTIVE);

    if (!gpio_is_ready_dt(&c->beam)) {
      return -ENODEV;
    }
    gpio_pin_configure_dt(&c->beam, GPIO_INPUT);
  }

  return 0;
}

enum dispenser_result dispense(enum cubby_id id) {
  if (!atomic_cas(&busy, 0, 1)) {
    return DISPENSER_ERR_BUSY;
  }

  const struct cubby *c = &cubbies[id];

  int beam_broken = poll_beam(&c->beam);
  enum phase curr_phase =
      beam_broken ? MOVE_UNTIL_BAR_CLEARS_BEAM : MOVE_UNTIL_BAR_BREAKS_BEAM;

  gpio_pin_set_dt(&c->motor, 1);

  int wait_budget = WAIT_MAX_MS;
  enum dispenser_result res = DISPENSER_OK;

  while (curr_phase < STOP) {
    bool cond_to_proceed = next_phase_beam_cond[curr_phase];

    while (wait_budget > 0) {
      beam_broken = poll_beam(&c->beam);
      if (beam_broken == cond_to_proceed) {
        break;
      }

      wait_budget--;
      k_msleep(1);
    }

    if (wait_budget <= 0) {
      res = DISPENSER_ERR_TIMEOUT;
      curr_phase = STOP;
      break;
    }

    curr_phase++;
  };

  gpio_pin_set_dt(&c->motor, 0);

  atomic_clear(&busy);

  return res;
}
