// hook_switch.c — Lift/replace detection via GPIO

#include "esp_log.h"

static const char *TAG = "hook_switch";

void hook_switch_init(void)
{
    ESP_LOGI(TAG, "hook_switch_init stub");
    // TODO: Configure GPIO for hook switch, set up ISR with debounce
}
