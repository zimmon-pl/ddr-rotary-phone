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

// Start call audio: reconfigure I2S for voice, unmute DAC
// sample_rate: 8000 for CVSD, 16000 for mSBC
esp_err_t audio_start_call(uint32_t sample_rate);

// Stop call audio: mute DAC, reconfigure I2S back to 44100Hz stereo
esp_err_t audio_stop_call(void);

// Write incoming call audio (mono 16-bit PCM) — called from BT task
void audio_write_call_data(const uint8_t *data, uint32_t len);

#endif
