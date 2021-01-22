#include "x1-buttons.h"
#include "../MarlinCore.h"
#include "../sd/cardreader.h"
#include "../gcode/queue.h"
#include "../module/temperature.h"
#include "../module/motion.h"

#if ENABLED(X1_BUTTONS)

#define STARTK_PIN 27
#define STARTK_LED_PIN 10
#define PLUSK_PIN 28
#define PLUSK_LED_PIN 11
#define MINUSK_PIN 29
#define MINUSK_LED_PIN 16
#define HOMEK_PIN 30
#define HOMEK_LED_PIN 17

#define X1_PREHEAT_TEMP 220
#define X1_PREHEAT_TEMP_STR "220"

PGMSTR(X1_PREHEAT_STR,  "M104 S" X1_PREHEAT_TEMP_STR);
PGMSTR(X1_PREHEAT_STOP, "M104 S0");
PGMSTR(X1_RELATIVE_EXTRUDER_STR, "M83");
PGMSTR(X1_FEED_STR, "G1 E1 F600");
PGMSTR(X1_RETRACT_STR, "G1 E-1 F600");
PGMSTR(X1_G90_STR, "G90");

PGMSTR(X1_LEVELING_0, "G0 X25 Y25 Z0 F1400");
PGMSTR(X1_LEVELING_1, "G0 X75 Y25 Z0 F1400");
PGMSTR(X1_LEVELING_2, "G0 X75 Y75 Z0 F1400");
PGMSTR(X1_LEVELING_3, "G0 X25 Y75 Z0 F1400");

#define X1_BLINK_PERIOD 500
#define X1_FAST_BLINK_PERIOD 75

static unsigned long x1_next_blink = 0UL;
static unsigned long x1_next_fast_blink = 0UL;

static unsigned long x1_next_home_key = 0UL;
static unsigned long x1_next_plus_key = 0UL;
static unsigned long x1_next_minus_key = 0UL;
static unsigned long x1_next_start_key = 0UL;

static short x1_startk_led_state = LOW;
static short x1_plusk_led_state  = HIGH;
static short x1_minusk_led_state = HIGH;
static short x1_homek_led_state  = HIGH;

// This is used to track the current leveling position. 0 refers
// to the home location, and they are indexed from 0 to 3 in 
// counter-clockwise order. 
static short x1_leveling_pos = 0;

typedef enum {
  IDLE,
  PRINTING,
  PREHEATING_FEED,
  PREHEATING_RETRACT,
  FEEDING,
  RETRACTING,
  LEVELING
} x1_state_t;

static x1_state_t x1_state;

void x1_setup() {
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

    // Reset the initial state
    x1_state = IDLE;
}

/*
 * Change the state of the led to something else, if new state
 * is specified. If newstate is left empty, then the LED is 
 * toggled depending on its current state. 
 */
void x1_toggle_start_led(short newstate = 2) {
  if (newstate == 2)
    newstate = !x1_startk_led_state;

  if (newstate != x1_startk_led_state)
  {
    x1_startk_led_state = newstate;
    WRITE(STARTK_LED_PIN, x1_startk_led_state);
  } 
}

void x1_toggle_minus_led(short newstate = 2) {
  if (newstate == 2)
    newstate = !x1_minusk_led_state;

  if (newstate != x1_minusk_led_state)
  {
    x1_minusk_led_state = newstate;
    WRITE(MINUSK_LED_PIN, x1_minusk_led_state);
  } 
}

void x1_toggle_home_led(short newstate = 2) {
  if (newstate == 2)
    newstate = !x1_homek_led_state;

  if (newstate != x1_homek_led_state)
  {
    x1_homek_led_state = newstate;
    WRITE(HOMEK_LED_PIN, x1_homek_led_state);
  } 
}

void x1_toggle_plus_led(short newstate = 2) {
  if (newstate == 2)
    newstate = !x1_plusk_led_state;

  if (newstate != x1_plusk_led_state)
  {
    x1_plusk_led_state = newstate;
    WRITE(PLUSK_LED_PIN, x1_plusk_led_state);
  } 
}

void x1_leds(const millis_t now) {
  bool need_blink = ELAPSED(now, x1_next_blink);
  bool need_fast_blink = ELAPSED(now, x1_next_fast_blink);

  if (need_blink) {
    x1_next_blink = now + X1_BLINK_PERIOD;
  }

  if (need_fast_blink) {
    x1_next_fast_blink = now + X1_FAST_BLINK_PERIOD;
  }

  switch (x1_state) {
    case IDLE:
      x1_toggle_start_led(LOW);
      x1_toggle_plus_led(HIGH);
      x1_toggle_minus_led(HIGH);
      x1_toggle_home_led(HIGH);
      break;

    case PRINTING:
      if (need_blink) {
        x1_toggle_start_led();
      }
      x1_toggle_plus_led(HIGH);
      x1_toggle_minus_led(HIGH);
      x1_toggle_home_led(HIGH);      
      break;

    case PREHEATING_FEED:
      x1_toggle_start_led(LOW);
      if (need_fast_blink) {
        x1_toggle_plus_led();
      }
      x1_toggle_minus_led(HIGH);
      x1_toggle_home_led(HIGH);
      break;

    case FEEDING:
      x1_toggle_start_led(LOW);
      if (need_blink) {
        x1_toggle_plus_led();
      }
      x1_toggle_minus_led(HIGH);
      x1_toggle_home_led(HIGH);

    case PREHEATING_RETRACT:
      x1_toggle_start_led(LOW);
      x1_toggle_plus_led(HIGH);
      if (need_fast_blink) {
        x1_toggle_minus_led();
      }
      x1_toggle_home_led(HIGH);
      break;

    case RETRACTING:
      x1_toggle_start_led(LOW);
      x1_toggle_plus_led(HIGH);
      if (need_blink) {
        x1_toggle_minus_led();
      }
      x1_toggle_home_led(HIGH);
      break;

    case LEVELING:
      x1_toggle_start_led(LOW);
      x1_toggle_plus_led(HIGH);
      x1_toggle_minus_led(HIGH);
      x1_toggle_home_led(LOW);
      break;
  }
}

void x1_leveling_move() {
  if (x1_state == LEVELING) {
    switch (x1_leveling_pos) {
      case 0:
        queue.enqueue_one_P(X1_LEVELING_0);
        break;
      case 1:
        queue.enqueue_one_P(X1_LEVELING_1);
        break;
      case 2:
        queue.enqueue_one_P(X1_LEVELING_2);
        break;
      case 3:
        queue.enqueue_one_P(X1_LEVELING_3);
        break;  
    }
  }
}

void x1_home_key_pressed() {
  switch (x1_state) {
    case IDLE:
      queue.enqueue_now_P(G28_STR);
      queue.enqueue_now_P(X1_G90_STR);
      x1_state = LEVELING;
      break;

    case LEVELING:
      queue.enqueue_now_P(G28_STR);
      x1_state = IDLE;
      break;

    default:
      break;
  } 
}

void x1_plus_key_pressed() {
  switch (x1_state) {
    case IDLE:
      x1_state = PREHEATING_FEED;
      queue.enqueue_now_P(X1_PREHEAT_STR);
      break;

    case PREHEATING_FEED:
    case FEEDING:
      // Turn off heater, and change state to generic
      x1_state = IDLE;
      quickstop_stepper();
      queue.enqueue_now_P(X1_PREHEAT_STOP);
      break;

    case LEVELING:
      // Move to the next point
      x1_leveling_pos = (x1_leveling_pos + 1) % 4;
      x1_leveling_move();
      break;

    default:
      break;
  }
}

void x1_minus_key_pressed() {
  switch (x1_state) {
    case IDLE:
      x1_state = PREHEATING_RETRACT;
      queue.enqueue_now_P(X1_PREHEAT_STR);
      break;

    case PREHEATING_RETRACT:
    case RETRACTING:
      // Turn off heater, and change state to generic
      x1_state = IDLE;
      quickstop_stepper();
      queue.enqueue_now_P(X1_PREHEAT_STOP);
      break;

    case LEVELING:
      // Move to the previous point
      x1_leveling_pos = (x1_leveling_pos + 5) % 4;
      x1_leveling_move();
      break;      

    default:
      break;
  }
}

void x1_start_key_pressed() {
  // FIXME: Not yet implemented
}

void x1_buttons(millis_t now) {
  constexpr millis_t debounce_delay = 1000UL;

  // Home key
  if (!READ(HOMEK_PIN)) {
    if (ELAPSED(now, x1_next_home_key)) {
      x1_home_key_pressed();
      x1_next_home_key = now + debounce_delay;
    }
  }

  // Plus key
  if (!READ(PLUSK_PIN)) {
    if (ELAPSED(now, x1_next_plus_key)) {
      x1_plus_key_pressed();
      x1_next_plus_key = now + debounce_delay;
    }
  }

  // Minus key
  if (!READ(MINUSK_PIN)) {
    if (ELAPSED(now, x1_next_minus_key)) {
      x1_minus_key_pressed();
      x1_next_minus_key = now + debounce_delay;
    }
  }

  // Start key
  if (!READ(STARTK_PIN)) {
    if (ELAPSED(now, x1_next_start_key)) {
      x1_start_key_pressed();
      x1_next_start_key = now + debounce_delay;
    }
  }    
}

void x1_switch_state(millis_t now) {
  bool printing = printingIsActive();

  // If a print operation has started we stop messing with
  // the printer, and go back to the generic state. 
  if (printing) {
    x1_state = PRINTING;
    return;
  }

  switch (x1_state) {
    case PREHEATING_FEED:
      // Check if we have reached the desired temperature
      if (thermalManager.degHotend(0) >= X1_PREHEAT_TEMP - 1) {
        queue.enqueue_one_P(X1_RELATIVE_EXTRUDER_STR);
        x1_state = FEEDING;
      }
      break;
    case PREHEATING_RETRACT:
      // Check if we have reached the desired temperature
      if (thermalManager.degHotend(0) >= X1_PREHEAT_TEMP - 1) {
        queue.enqueue_one_P(X1_RELATIVE_EXTRUDER_STR);

        // Before retracting we do a short feed move, which
        // helps with truncating the filament. 
        queue.enqueue_one_P(X1_FEED_STR);
        
        x1_state = RETRACTING;
      }
      break;

    case PRINTING:
      if (! printing) {
        x1_state = IDLE;
      }
      break;

    default:
      break;
  }
}

void x1_feed() {
  switch (x1_state) {
    case FEEDING:
      // If we are about to end the commands in the queue, then 
      // push a few more extrusion ones. 
      if (! queue.has_commands_queued()) {
        queue.enqueue_one_P(X1_FEED_STR);
      }
      break;
    case RETRACTING:
      // If we are about to end the commands in the queue, then 
      // push a few more extrusion ones. 
      if (! queue.has_commands_queued()) {
        queue.enqueue_now_P(X1_RETRACT_STR);
      }
      break;      
    default:
      break;
  }
}

void x1_idle() {
  const millis_t now = millis();
  x1_buttons(now);
  x1_switch_state(now);
  x1_leds(now);
  x1_feed();
}

#endif // X1_BUTTONS