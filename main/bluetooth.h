// bluetooth.h — HFP Hands-Free Profile interface

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdbool.h>
#include "esp_err.h"

// Init BT stack, register HFP, set name "FestStefan", make discoverable
esp_err_t bluetooth_init(void);

// Answer a ringing incoming call (AT+ATA)
esp_err_t bluetooth_answer_call(void);

// Hang up the current call, whether ringing or active (AT+CHUP)
esp_err_t bluetooth_hangup_call(void);

// Place an outgoing call to a phone number string (e.g. "0301234567").
// Routed via HFP AT+ATD on the paired phone. No-op if HFP isn't connected.
esp_err_t bluetooth_dial_number(const char *number);

// True while the phone is signaling an incoming call
bool bluetooth_is_ringing(void);

// True while an HFP call is in progress
bool bluetooth_is_in_call(void);

// True once the HFP Service Level Connection is up (paired phone, ready for calls)
bool bluetooth_is_connected(void);

// True during any active call-setup phase: incoming ring, outgoing dialing,
// or outgoing alerting. Used by the LED to blink while a call is "in flux".
bool bluetooth_is_call_setup(void);

// Tell the BT stack we have outgoing audio data ready. Without this nudge the
// stack never invokes the registered outgoing data callback. Call after every
// push to the mic ring buffer while a call is active.
void bluetooth_notify_outgoing_ready(void);

#endif
