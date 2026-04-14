// audio.h — ES8388 codec + I2S audio interface

#ifndef AUDIO_H
#define AUDIO_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Init I2C, ES8388 codec, I2S, and PA enable
esp_err_t audio_init(void);

// Play a sine wave tone at given frequency for given duration
esp_err_t audio_play_tone(uint32_t freq_hz, uint32_t duration_ms);

// Set volume 0-100
esp_err_t audio_set_volume(uint8_t volume);

// Mute or unmute DAC output
esp_err_t audio_set_mute(bool mute);

#endif
