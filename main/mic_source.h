// mic_source.h — pluggable microphone source for HFP outgoing audio
//
// HFP outgoing audio comes from a mic source. Today there is one: the ES8388
// codec via I2S RX (mic_source_codec). A future implementation will read from
// the ESP32 internal ADC (mic_source_adc1) so we can bypass the A1S V2.2
// LINEIN/MIC short documented in DIARY.md (2026-04-24).
//
// app_main picks one source via mic_source_set() once at boot. audio_start_call
// / audio_stop_call drive its lifecycle. bluetooth.c reads from it via
// mic_source_read() inside the HFP outgoing data callback.

#ifndef MIC_SOURCE_H
#define MIC_SOURCE_H

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    const char *name;
    esp_err_t (*start)(uint32_t sample_rate);
    esp_err_t (*stop)(void);
    uint32_t  (*read)(uint8_t *buf, uint32_t len);
} mic_source_t;

// Choose the active source. Call once at boot, before audio_start_call.
void mic_source_set(const mic_source_t *src);

// Lifecycle wrappers — dispatch to the active source. Safe to call when no
// source is set (start/stop return ESP_OK; read zero-pads buf).
esp_err_t mic_source_start(uint32_t sample_rate);
esp_err_t mic_source_stop(void);
uint32_t  mic_source_read(uint8_t *buf, uint32_t len);

// Available implementations
extern const mic_source_t mic_source_codec;

#endif
