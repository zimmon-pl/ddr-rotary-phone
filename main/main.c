// main.c — FestStefan tone test
//
// Task 1: Confirm audio works by playing a 440Hz beep through headphones.
// Plug headphones into 3.5mm jack, flash, listen for the beep.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio.h"

static const char *TAG = "main";

void app_main(void)
{
    printf("\n*** FestStefan — Tone Test ***\n\n");

    esp_err_t ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed — check I2C/codec connection");
        return;
    }

    // Play 440Hz tone for 2 seconds
    ESP_LOGI(TAG, "Playing test tone — listen on headphones!");
    audio_play_tone(440, 2000);

    // Play a second tone at a different pitch so you know it's intentional
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Playing second tone (880Hz)...");
    audio_play_tone(880, 1000);

    // Mute after test — I2S DMA loops the last buffer otherwise
    audio_set_mute(true);

    ESP_LOGI(TAG, "TONE TEST COMPLETE — if you heard two beeps, audio works!");

    // Keep running (don't exit app_main on ESP32)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
