#include "dispenser.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>

#define DELAY_MS 1
#define WAIT_MAX_MS 5000

struct cubby {
  const struct gpio_dt_spec motor;
  const struct gpio_dt_spec beam;
};

#define CUBBY(motor_alias, beam_alias)                                         \
  {.motor = GPIO_DT_SPEC_GET(DT_ALIAS(motor_alias), gpios),                    \
   .beam = GPIO_DT_SPEC_GET(DT_ALIAS(beam_alias), gpios)}

static const struct cubby cubbies[NUM_CUBBIES] = {
    [POS_A] = CUBBY(motora, beama), [POS_B] = CUBBY(motorb, beamb),
    [POS_C] = CUBBY(motorc, beamc), [POS_D] = CUBBY(motord, beamd),
    [POS_E] = CUBBY(motore, beame), [POS_F] = CUBBY(motorf, beamf)};

static const struct led_dt_spec led = LED_DT_SPEC_GET(DT_ALIAS(led));

enum phase {
  MOVE_UNTIL_BAR_BREAKS_BEAM,
  MOVE_UNTIL_BAR_CLEARS_BEAM,
  STOP,
};

static const bool next_phase_beam_cond[2] = {
    [MOVE_UNTIL_BAR_BREAKS_BEAM] = true,
    [MOVE_UNTIL_BAR_CLEARS_BEAM] = false,
};

struct dispenser {
  enum phase curr_phase;
  atomic_t busy;
};

static struct dispenser d;

/*
  Helpers
*/

void poll_beam(const struct gpio_dt_spec *beam, int *reading) {
  *reading = gpio_pin_get_dt(beam);
  *reading ? led_on_dt(&led) : led_off_dt(&led);
}

// returns true if motor should continue for next phase
// false if end of cycle
bool move_until_next_phase(const struct gpio_dt_spec *beam) {
  if (d.curr_phase >= STOP) {
    return false;
  }

  bool cond_to_proceed = next_phase_beam_cond[d.curr_phase];

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

  d.curr_phase++;
  return true;
}

/*
  Public API
*/

int dispenser_init(void) {
  atomic_clear(&d.busy);
  d.curr_phase = STOP;

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

enum dispenser_result dispense(enum position pos) {
  if (atomic_get(&d.busy)) {
    return DISPENSER_ERR_BUSY;
  }

  atomic_set(&d.busy, 1);

  int beam_broken;

  const struct cubby *c = &cubbies[pos];

  poll_beam(&c->beam, &beam_broken);
  d.curr_phase =
      beam_broken ? MOVE_UNTIL_BAR_CLEARS_BEAM : MOVE_UNTIL_BAR_BREAKS_BEAM;

  gpio_pin_set_dt(&c->motor, 1);
  while (move_until_next_phase(&c->beam))
    ;
  gpio_pin_set_dt(&c->motor, 0);
  poll_beam(&c->beam, &beam_broken);

  atomic_clear(&d.busy);
  return DISPENSER_OK;
}
