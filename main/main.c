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
#include "hook_switch.h"

static const char *TAG = "main";

static void on_hook_change(hook_state_t state)
{
    if (state == HOOK_OFF) {
        // Handset lifted: answer if ringing, otherwise play dial tone.
        if (bluetooth_is_ringing()) {
            bluetooth_answer_call();
        } else {
            audio_start_dial_tone();
        }
    } else {
        // Handset replaced: stop dial tone (harmless if not playing) and
        // hang up any ongoing/ringing call.
        audio_stop_dial_tone();
        if (bluetooth_is_ringing() || bluetooth_is_in_call()) {
            bluetooth_hangup_call();
        }
    }
}

void app_main(void)
{
    printf("\n*** FestStefan ***\n\n");

    // Init audio (ES8388 + I2S)
    esp_err_t ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed");
        return;
    }

    // Digital DAC volume at 100 (no attenuation) — LOUT1VOL in audio.c
    // provides the analog attenuation for the handset speaker. Voice samples
    // are low-amplitude, so we need the digital headroom.
    audio_set_volume(100);

    // Carbon mic needs high PGA gain — 0xBB ≈ 33 dB L+R. Tune down if saturating.
    audio_set_mic_gain(0xBB);

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

    // Hook switch: detect lift/replace, drive call answer/hangup.
    hook_switch_init();
    hook_switch_on_change(on_hook_change);

    ESP_LOGI(TAG, "FestStefan is ready — open Bluetooth settings on your phone!");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
