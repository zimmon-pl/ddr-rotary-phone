// main.c — DDR Rotary Phone Bluetooth Headset
// Entry point: initializes NVS, Bluetooth stack, and all hardware modules.

#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

// Module init declarations
void bluetooth_init(void);
void audio_init(void);
void rotary_dial_init(void);
void hook_switch_init(void);
void dial_actions_init(void);
void led_init(void);
void tones_init(void);

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "DDR Rotary Phone starting...");

    // 1. Initialize NVS (required by Bluetooth stack)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // 2. Release BLE memory (we only use Classic BT)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // 3. Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_LOGI(TAG, "BT controller enabled (Classic only)");

    // 4. Initialize Bluedroid stack
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_LOGI(TAG, "Bluedroid enabled");

    // 5. Initialize hardware modules
    audio_init();
    ESP_LOGI(TAG, "Audio initialized");

    led_init();
    ESP_LOGI(TAG, "LED initialized");

    tones_init();
    ESP_LOGI(TAG, "Tones initialized");

    hook_switch_init();
    ESP_LOGI(TAG, "Hook switch initialized");

    rotary_dial_init();
    ESP_LOGI(TAG, "Rotary dial initialized");

    dial_actions_init();
    ESP_LOGI(TAG, "Dial actions initialized");

    // 6. Initialize Bluetooth HFP (after Bluedroid is up)
    bluetooth_init();
    ESP_LOGI(TAG, "Bluetooth HFP initialized");

    ESP_LOGI(TAG, "All modules initialized — ready");
}
