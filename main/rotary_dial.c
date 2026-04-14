// rotary_dial.c — Pulse counting, digit decoding, 2s timeout
//
// How a rotary dial works:
//   1. User pulls dial to a digit and releases
//   2. Dial spins back — the off-normal (nsr) signal goes active
//   3. As it spins, the pulse (nsi) contact opens/closes once per count
//      (dial 3 = 3 pulses, dial 0 = 10 pulses)
//   4. Dial reaches rest — off-normal goes inactive, digit is complete
//   5. After 2 seconds of no new digit, the full number is sent
//
// Pulse timing (European standard): ~10 pulses/sec, 60ms break + 40ms make

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rotary_dial.h"

static const char *TAG = "rotary_dial";

// ---------------------------------------------------------------------------
// GPIO pin assignments — change these when board arrives and wiring is known
// Using GPIO16/17 — free on A1S AudioKit board
// ---------------------------------------------------------------------------
#define ROTARY_PULSE_PIN      GPIO_NUM_16   // nsi — pulse contact
#define ROTARY_OFF_NORMAL_PIN GPIO_NUM_17   // nsr — off-normal contact

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
#define DEBOUNCE_US           20000   // 20ms — ignore bounces shorter than this
#define NUMBER_TIMEOUT_US     2000000 // 2 seconds — number complete after this silence
#define MAX_NUMBER_LENGTH     16      // max digits in a phone number

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static volatile uint8_t pulse_count = 0;         // pulses counted for current digit
static volatile int64_t last_pulse_time_us = 0;   // timestamp of last pulse (for debounce)
static volatile bool dial_active = false;         // true while dial is spinning

static char number_buffer[MAX_NUMBER_LENGTH + 1]; // collected digits as string
static uint8_t number_length = 0;                 // how many digits collected so far

static esp_timer_handle_t number_timeout_timer;   // fires 2s after last digit
static QueueHandle_t event_queue;                 // ISR sends events here for task to process

// Callbacks registered by other modules
static rotary_digit_cb_t digit_callback = NULL;
static rotary_number_cb_t number_callback = NULL;

// Events sent from ISR to the processing task
typedef enum {
    EVT_DIAL_START,    // off-normal went active (dial left rest)
    EVT_PULSE,         // one pulse detected
    EVT_DIAL_END,      // off-normal went inactive (dial returned to rest)
} rotary_event_t;

// ---------------------------------------------------------------------------
// Interrupt handlers — these run when the GPIO pins change state
// They must be very short, so they just put an event on the queue
// ---------------------------------------------------------------------------
static void IRAM_ATTR pulse_isr_handler(void *arg)
{
    // Only count falling edges (pulse break), and debounce
    int64_t now = esp_timer_get_time();
    if ((now - last_pulse_time_us) > DEBOUNCE_US) {
        last_pulse_time_us = now;
        rotary_event_t evt = EVT_PULSE;
        xQueueSendFromISR(event_queue, &evt, NULL);
    }
}

static void IRAM_ATTR off_normal_isr_handler(void *arg)
{
    int level = gpio_get_level(ROTARY_OFF_NORMAL_PIN);
    rotary_event_t evt = (level == 0) ? EVT_DIAL_START : EVT_DIAL_END;
    xQueueSendFromISR(event_queue, &evt, NULL);
}

// ---------------------------------------------------------------------------
// Timer callback — fires 2 seconds after the last digit
// ---------------------------------------------------------------------------
static void number_timeout_cb(void *arg)
{
    if (number_length > 0 && number_callback != NULL) {
        ESP_LOGI(TAG, "Number complete: %s (%d digits)", number_buffer, number_length);
        number_callback(number_buffer, number_length);
    }
    // Reset for next number
    number_length = 0;
    number_buffer[0] = '\0';
}

// ---------------------------------------------------------------------------
// Main task — processes events from the ISR queue
// ---------------------------------------------------------------------------
static void rotary_dial_task(void *arg)
{
    rotary_event_t evt;

    for (;;) {
        // Wait for an event from the ISRs (blocks until something happens)
        if (xQueueReceive(event_queue, &evt, portMAX_DELAY)) {
            switch (evt) {

            case EVT_DIAL_START:
                // Dial just left rest position — get ready to count pulses
                dial_active = true;
                pulse_count = 0;
                // Stop the number timeout while user is dialing
                esp_timer_stop(number_timeout_timer);
                ESP_LOGD(TAG, "Dial active");
                break;

            case EVT_PULSE:
                // One pulse detected
                if (dial_active) {
                    pulse_count++;
                    ESP_LOGD(TAG, "Pulse %d", pulse_count);
                }
                break;

            case EVT_DIAL_END:
                // Dial returned to rest — decode the digit
                if (dial_active && pulse_count > 0) {
                    uint8_t digit = pulse_count % 10; // 10 pulses = digit 0
                    dial_active = false;

                    ESP_LOGI(TAG, "Digit decoded: %d (%d pulses)", digit, pulse_count);

                    // Add to number buffer
                    if (number_length < MAX_NUMBER_LENGTH) {
                        number_buffer[number_length] = '0' + digit;
                        number_length++;
                        number_buffer[number_length] = '\0';
                        ESP_LOGI(TAG, "Number so far: %s", number_buffer);
                    }

                    // Notify digit callback
                    if (digit_callback != NULL) {
                        digit_callback(digit);
                    }

                    // Start (or restart) the 2-second timeout
                    esp_timer_stop(number_timeout_timer);
                    esp_timer_start_once(number_timeout_timer, NUMBER_TIMEOUT_US);
                }
                dial_active = false;
                pulse_count = 0;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

void rotary_dial_init(void)
{
    ESP_LOGI(TAG, "Initializing rotary dial (pulse=GPIO%d, off-normal=GPIO%d)",
             ROTARY_PULSE_PIN, ROTARY_OFF_NORMAL_PIN);

    // Create event queue (holds up to 20 events — plenty for fast pulses)
    event_queue = xQueueCreate(20, sizeof(rotary_event_t));

    // Reset number buffer
    number_length = 0;
    number_buffer[0] = '\0';

    // --- Configure pulse pin (nsi) ---
    // Pull-up enabled: the contact is normally closed, pulses are breaks (goes HIGH)
    gpio_config_t pulse_cfg = {
        .pin_bit_mask = (1ULL << ROTARY_PULSE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,  // trigger on falling edge (end of break)
    };
    gpio_config(&pulse_cfg);

    // --- Configure off-normal pin (nsr) ---
    // Pull-up enabled: active when dial moves, returns high at rest
    gpio_config_t off_normal_cfg = {
        .pin_bit_mask = (1ULL << ROTARY_OFF_NORMAL_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,  // trigger on both edges (start and end)
    };
    gpio_config(&off_normal_cfg);

    // --- Set up interrupt handlers ---
    // (ok if already installed by another module — will just return an error we ignore)
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ROTARY_PULSE_PIN, pulse_isr_handler, NULL);
    gpio_isr_handler_add(ROTARY_OFF_NORMAL_PIN, off_normal_isr_handler, NULL);

    // --- Create the 2-second timeout timer ---
    esp_timer_create_args_t timer_args = {
        .callback = number_timeout_cb,
        .name = "dial_timeout",
    };
    esp_timer_create(&timer_args, &number_timeout_timer);

    // --- Start the task that processes events ---
    xTaskCreate(rotary_dial_task, "rotary_dial", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Rotary dial ready");
}

void rotary_dial_on_digit(rotary_digit_cb_t cb)
{
    digit_callback = cb;
}

void rotary_dial_on_number(rotary_number_cb_t cb)
{
    number_callback = cb;
}

void rotary_dial_clear(void)
{
    esp_timer_stop(number_timeout_timer);
    number_length = 0;
    number_buffer[0] = '\0';
    pulse_count = 0;
    dial_active = false;
    ESP_LOGI(TAG, "Dial buffer cleared");
}
