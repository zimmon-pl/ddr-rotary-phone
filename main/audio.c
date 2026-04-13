// audio.c — ES8388 codec config, I2S, mic/speaker

#include "esp_log.h"

static const char *TAG = "audio";

void audio_init(void)
{
    ESP_LOGI(TAG, "audio_init stub");
    // TODO: Configure ES8388 codec via I2C, set up I2S for mic and speaker
}
