// rotary_dial.c — Pulse counting, digit decoding, 2s timeout

#include "esp_log.h"

static const char *TAG = "rotary_dial";

void rotary_dial_init(void)
{
    ESP_LOGI(TAG, "rotary_dial_init stub");
    // TODO: Configure GPIO for pulse input, set up ISR and timer for digit timeout
}
