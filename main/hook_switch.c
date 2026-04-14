// hook_switch.c — Lift/replace detection via GPIO
//
// The hook switch is pressed down by the weight of the handset.
// Lifting the handset releases the switch.
//
// With a pull-up resistor:
//   Handset DOWN → switch closed → GPIO reads LOW (0)
//   Handset UP   → switch open  → GPIO reads HIGH (1)
//
// We debounce with a 50ms delay — mechanical switches bounce a bit
// when the handset is slammed down or picked up quickly.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "hook_switch.h"

static const char *TAG = "hook_switch";

// ---------------------------------------------------------------------------
// GPIO pin assignment — change when board arrives
// ---------------------------------------------------------------------------
#define HOOK_SWITCH_PIN  GPIO_NUM_4   // Free on A1S AudioKit board

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define DEBOUNCE_MS  50  // ignore bounces shorter than 50ms

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static hook_state_t current_state = HOOK_ON;  // assume handset is down at startup
static hook_change_cb_t change_callback = NULL;
static QueueHandle_t event_queue;

// ---------------------------------------------------------------------------
// Interrupt handler — pin changed, start debounce timer
// ---------------------------------------------------------------------------
static void IRAM_ATTR hook_isr_handler(void *arg)
{
    // Send event to task — don't call esp_timer from ISR (crashes in Wokwi)
    hook_state_t evt = HOOK_ON;  // dummy value, task reads pin directly
    xQueueSendFromISR(event_queue, &evt, NULL);
}

// ---------------------------------------------------------------------------
// Main task — waits for debounced events
// ---------------------------------------------------------------------------
static void hook_switch_task(void *arg)
{
    hook_state_t evt;

    for (;;) {
        if (xQueueReceive(event_queue, &evt, portMAX_DELAY)) {
            // Debounce: wait, then read the settled pin level
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            // Drain any extra events that arrived during debounce
            while (xQueueReceive(event_queue, &evt, 0)) {}

            int level = gpio_get_level(HOOK_SWITCH_PIN);
            hook_state_t new_state = (level == 1) ? HOOK_OFF : HOOK_ON;

            if (new_state != current_state) {
                current_state = new_state;

                if (new_state == HOOK_OFF) {
                    ESP_LOGI(TAG, "Handset LIFTED");
                } else {
                    ESP_LOGI(TAG, "Handset REPLACED");
                }

                if (change_callback != NULL) {
                    change_callback(current_state);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

void hook_switch_init(void)
{
    ESP_LOGI(TAG, "Initializing hook switch (GPIO%d)", HOOK_SWITCH_PIN);

    event_queue = xQueueCreate(5, sizeof(hook_state_t));

    // Configure GPIO with pull-up
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << HOOK_SWITCH_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&cfg);

    // Read initial state
    int level = gpio_get_level(HOOK_SWITCH_PIN);
    current_state = (level == 1) ? HOOK_OFF : HOOK_ON;
    ESP_LOGI(TAG, "Initial state: %s", (current_state == HOOK_OFF) ? "LIFTED" : "ON CRADLE");

    // Install ISR service if not already installed, then add our handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(HOOK_SWITCH_PIN, hook_isr_handler, NULL);

    // Start processing task
    xTaskCreate(hook_switch_task, "hook_switch", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Hook switch ready");
}

void hook_switch_on_change(hook_change_cb_t cb)
{
    change_callback = cb;
}

hook_state_t hook_switch_get_state(void)
{
    return current_state;
}
