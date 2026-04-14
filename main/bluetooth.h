// bluetooth.h — HFP Hands-Free Profile interface

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "esp_err.h"

// Init BT stack, register HFP, set name "FestStefan", make discoverable
esp_err_t bluetooth_init(void);

#endif
