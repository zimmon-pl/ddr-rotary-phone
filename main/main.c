// main.c — FestStefan
//
// Step 2a: Audio tone test + Bluetooth HFP pairing
// Flash this, hear two beeps, then check your phone for "FestStefan"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "audio.h"
#include "bluetooth.h"
#include "hook_switch.h"
#include "led.h"
#include "mic_source.h"
#include "rotary_dial.h"
#include "dial_actions.h"

static const char *TAG = "main";

// On-board REC button — input-only pin with external pull-up, reads LOW
// when pressed. Used as a debug trigger for the mic test task.
#define REC_BUTTON_PIN  GPIO_NUM_36

static void rec_button_task(void *arg)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << REC_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,     // GPIO 36 has no internal pulls
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    int last = 1;
    for (;;) {
        int level = gpio_get_level(REC_BUTTON_PIN);
        if (level == 0 && last == 1) {
            vTaskDelay(pdMS_TO_TICKS(30));             // debounce
            if (gpio_get_level(REC_BUTTON_PIN) == 0) {
                if (audio_mic_test_active()) {
                    audio_stop_mic_test();
                } else {
                    audio_start_mic_test();
                }
            }
        }
        last = level;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

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
        // Handset replaced: stop dial tone, drop any half-dialled number,
        // and hang up any ongoing/ringing call.
        audio_stop_dial_tone();
        rotary_dial_clear();
        if (bluetooth_is_ringing() || bluetooth_is_in_call()) {
            bluetooth_hangup_call();
        }
    }
}

// First pulse of any digit means the user has started dialling — kill the
// dial tone so it doesn't play over the rotary clicks. Idempotent: safe to
// call on every digit.
static void on_digit(uint8_t digit)
{
    (void)digit;
    audio_stop_dial_tone();
}

void app_main(void)
{
    printf("\n*** FestStefan ***\n\n");

    // Status LED (GPIO 22) up first so we have visible feedback during boot
    led_init();

    // Default mic source: ES8388 codec via I2S RX. Once the ADC1 path lands we
    // can switch this line (or expose a build-time flag) without touching the
    // BT/HFP code path.
    mic_source_set(&mic_source_codec);

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

    // Electret + bias tee is giving us low signal levels (~1700 peak on taps).
    // Crank PGA to max (0xFF) to see if real speech lifts above ~320 RMS floor.
    audio_set_mic_gain(0xFF);

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

    // Rotary dial: count pulses, decode digits, dial out on number-complete.
    // Wires brown→GPIO 16 (pulse), white→GPIO 17 (off-normal), green→GND.
    rotary_dial_init();
    rotary_dial_on_digit(on_digit);
    rotary_dial_on_number(dial_actions_handle_number);

    // REC button toggles a mic-level debug log (press once to start, again to stop)
    xTaskCreate(rec_button_task, "rec_button", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "FestStefan is ready — open Bluetooth settings on your phone!");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
