#include "x1-buttons.h"
#include "../MarlinCore.h"

#ifdef X1_BUTTONS

#define X1_BLINK_PERIOD 500
#define X1_FAST_BLINK_PERIOD 100

static unsigned long x1_next_blink = 0L;
static unsigned long x1_next_fast_blink = 0L;

static short x1_startk_led_state = LOW;
static short x1_plusk_led_state  = HIGH;
static short x1_minusk_led_state = HIGH;
static short x1_homek_led_state  = HIGH;

void x1_setup()
{
    SET_INPUT_PULLUP(HOMEK_PIN);
    SET_INPUT_PULLUP(PLUSK_PIN);
    SET_INPUT_PULLUP(MINUSK_PIN);
    SET_INPUT_PULLUP(STARTK_PIN);

    SET_OUTPUT(HOMEK_LED_PIN);
    SET_OUTPUT(PLUSK_LED_PIN);
    SET_OUTPUT(MINUSK_LED_PIN);
    SET_OUTPUT(STARTK_LED_PIN);

    OUT_WRITE(HOMEK_LED_PIN, x1_homek_led_state);
    OUT_WRITE(PLUSK_LED_PIN, x1_plusk_led_state);
    OUT_WRITE(MINUSK_LED_PIN, x1_minusk_led_state);
    OUT_WRITE(STARTK_LED_PIN, x1_startk_led_state);
}

/*
 * Change the state of the led to something else, if new state
 * is specified. If newstate is left empty, then the LED is 
 * toggled depending on its current state. 
 */
void x1_toggle_start_led(short newstate = 2)
{
  if (newstate == 2)
    newstate = !x1_startk_led_state;

  if (newstate != x1_startk_led_state)
  {
    x1_startk_led_state = newstate;
    WRITE(STARTK_LED_PIN, x1_startk_led_state);
  } 
}

void x1_leds(const millis_t now) {
  bool printActive = printingIsActive();

  // Handle any LED that is supposed to be blinking slowly
  if (ELAPSED(now, x1_next_blink)) {
    if (printActive)
      x1_toggle_start_led();

    x1_next_blink = now + X1_BLINK_PERIOD;
  }

  // Handle any fast blinking LED here
  if (ELAPSED(now, x1_next_fast_blink)) {
    x1_next_fast_blink = now + X1_FAST_BLINK_PERIOD;
  }
}

void x1_idle() {
  const millis_t now = millis();

  x1_leds(now);
}

#endif