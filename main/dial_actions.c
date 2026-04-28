// dial_actions.c — number → action routing
//
//   "999" → factory reset / re-pair (esp_restart)
//   else  → HFP dial via the paired phone

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "dial_actions.h"
#include "bluetooth.h"

static const char *TAG = "dial_actions";

void dial_actions_handle_number(const char *number, uint8_t length)
{
    if (number == NULL || length == 0) return;

    ESP_LOGI(TAG, "number complete: '%s' (%d digits)", number, length);

    if (length == 3 && strcmp(number, "999") == 0) {
        ESP_LOGW(TAG, "999 dialed — restarting in 1 s for BT re-pair");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    bluetooth_dial_number(number);   // logs + bails if HFP not connected
}
