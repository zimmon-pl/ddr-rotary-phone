// tones.c — Dial tone generation (440 Hz + 350 Hz sine wave mix)

#include "esp_log.h"

static const char *TAG = "tones";

void tones_init(void)
{
    ESP_LOGI(TAG, "tones_init stub");
    // TODO: Prepare sine wave buffers for dial tone playback
}
