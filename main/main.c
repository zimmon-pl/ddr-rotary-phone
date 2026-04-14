// main.c — FestStefan
//
// Step 2a: Audio tone test + Bluetooth HFP pairing
// Flash this, hear two beeps, then check your phone for "FestStefan"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio.h"
#include "bluetooth.h"

static const char *TAG = "main";

void app_main(void)
{
    printf("\n*** FestStefan ***\n\n");

    // Init audio (ES8388 + I2S)
    esp_err_t ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed");
        return;
    }

    // Play a quick tone to confirm audio still works
    ESP_LOGI(TAG, "Audio check — short beep...");
    audio_play_tone(440, 500);
    audio_set_mute(true);

    // Init Bluetooth HFP
    ret = bluetooth_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth init failed");
        return;
    }

    ESP_LOGI(TAG, "FestStefan is ready — open Bluetooth settings on your phone!");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
