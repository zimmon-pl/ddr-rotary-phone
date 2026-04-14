// rotary_dial.h — Public interface for rotary dial pulse detection
//
// The rotary dial has two signals:
//   1. Pulse (nsi) — one quick open/close per digit count as dial spins back
//   2. Off-normal (nsr) — active while dial is away from rest position
//
// Usage:
//   rotary_dial_init();
//   rotary_dial_on_digit(my_digit_callback);
//   rotary_dial_on_number(my_number_callback);

#ifndef ROTARY_DIAL_H
#define ROTARY_DIAL_H

#include <stdint.h>

// Called each time a single digit is decoded (0-9)
typedef void (*rotary_digit_cb_t)(uint8_t digit);

// Called when dialing is complete (2s timeout after last digit)
// number_str is null-terminated, e.g. "0301234567"
typedef void (*rotary_number_cb_t)(const char *number_str, uint8_t length);

// Set up GPIOs, interrupts, and timers for rotary dial detection
void rotary_dial_init(void);

// Register a callback for each decoded digit
void rotary_dial_on_digit(rotary_digit_cb_t cb);

// Register a callback for the completed phone number
void rotary_dial_on_number(rotary_number_cb_t cb);

// Clear any digits collected so far (e.g. when handset is replaced mid-dial)
void rotary_dial_clear(void);

#endif
