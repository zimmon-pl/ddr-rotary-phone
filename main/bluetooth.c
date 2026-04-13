// bluetooth.c — HFP: pairing, answer, hang up, dial

#include "esp_log.h"

static const char *TAG = "bluetooth";

void bluetooth_init(void)
{
    ESP_LOGI(TAG, "bluetooth_init stub");
    // TODO: Register HFP HF callbacks, set device name, make discoverable
}
