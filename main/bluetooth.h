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

// True while the phone is signaling an incoming call
bool bluetooth_is_ringing(void);

// True while an HFP call is in progress
bool bluetooth_is_in_call(void);

#endif
