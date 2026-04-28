// led.c — GPIO 22 status LED, driven from BT state
//
// GPIO 22 is the on-board green LED (D4) on the Ai-Thinker A1S V2.2. Drive
// HIGH = on. We run a small task that polls the Bluetooth state every 50 ms
// and sets the LED level according to a state-specific blink pattern.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led.h"
#include "bluetooth.h"

static const char *TAG = "led";

#define LED_PIN          GPIO_NUM_22
#define LED_TICK_MS      50

// On the A541 A1S board, GPIO 22's LED is active-low: GPIO LOW = LED on,
// HIGH = LED off. Confirmed empirically 2026-04-28 — "solid on" appeared
// off until we inverted the drive level.
#define LED_ACTIVE_LOW   1

typedef enum {
    LED_OFF        = 0,   // off (boot, before init)
    LED_DISCONN,          // 1 Hz blink — no BT
    LED_CONNECTED,        // solid on — paired, idle
    LED_SETUP,            // 4 Hz blink — incoming ring or outgoing dialing
    LED_IN_CALL,          // solid on — active call
} led_state_t;

static led_state_t derive_state(void)
{
    if (bluetooth_is_in_call())     return LED_IN_CALL;
    if (bluetooth_is_call_setup())  return LED_SETUP;
    if (bluetooth_is_connected())   return LED_CONNECTED;
    return LED_DISCONN;
}

static int level_for(led_state_t s, uint32_t tick)
{
    switch (s) {
    case LED_OFF:        return 0;
    case LED_CONNECTED:
    case LED_IN_CALL:    return 1;
    case LED_DISCONN:    return (tick % 20) < 10;   // 1 Hz @ 50 ms tick
    case LED_SETUP:      return (tick % 5)  < 2;    // ~4 Hz @ 50 ms tick
    }
    return 0;
}

static void led_task(void *arg)
{
    uint32_t tick = 0;
    for (;;) {
        led_state_t s = derive_state();
        int level = level_for(s, tick) ^ LED_ACTIVE_LOW;
        gpio_set_level(LED_PIN, level);
        tick++;
        vTaskDelay(pdMS_TO_TICKS(LED_TICK_MS));
    }
}

void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(LED_PIN, LED_ACTIVE_LOW);   // start with LED off

    xTaskCreate(led_task, "led_status", 2048, NULL, 2, NULL);
    ESP_LOGI(TAG, "status LED on GPIO %d", LED_PIN);
}
