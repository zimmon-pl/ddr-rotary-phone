// led.c — WS2812B ring, state machine

#include "esp_log.h"

static const char *TAG = "led";

void led_init(void)
{
    ESP_LOGI(TAG, "led_init stub");
    // TODO: Configure RMT driver for WS2812B, start LED task
}
