// audio.h — ES8388 codec + I2S audio interface

#ifndef AUDIO_H
#define AUDIO_H

#include "esp_err.h"
#include "driver/i2s_std.h"
#include <stdbool.h>
#include <stdint.h>

// Init I2C, ES8388 codec, I2S, and PA enable
esp_err_t audio_init(void);

// Play a sine wave tone at given frequency for given duration
esp_err_t audio_play_tone(uint32_t freq_hz, uint32_t duration_ms);

// Set DAC digital volume 0-100
esp_err_t audio_set_volume(uint8_t volume);

// Set handset speaker volume from HFP phone-side volume index (0-15).
// Maps to ES8388 LOUT1/ROUT1 analog volume (DACCONTROL24/25). Wire this to
// ESP_HF_CLIENT_VOLUME_CONTROL_EVT (type=TARGET_SPK) so the phone's volume
// keys actually change the earpiece loudness.
esp_err_t audio_set_lout_volume(uint8_t bt_volume);

// Set ADC/mic PGA gain — raw ADCCONTROL1 value (0x88 = 24dB, 0xBB ~ 33dB)
esp_err_t audio_set_mic_gain(uint8_t gain_reg);

// Mute or unmute DAC output
esp_err_t audio_set_mute(bool mute);

// Start call audio: reconfigure I2S for voice, unmute DAC, start mic capture
// sample_rate: 8000 for CVSD, 16000 for mSBC
esp_err_t audio_start_call(uint32_t sample_rate);

// Stop call audio: mute DAC, stop mic, reconfigure I2S back to 44100Hz stereo
esp_err_t audio_stop_call(void);

// Write incoming call audio (mono 16-bit PCM) — called from BT task
void audio_write_call_data(const uint8_t *data, uint32_t len);

// Shared I2S RX handle. Used by mic_source_codec to read from the ES8388 ADC.
// Valid after audio_init() returns ESP_OK.
i2s_chan_handle_t audio_get_i2s_rx_handle(void);

// Start the European dial tone (350 + 440 Hz sine mix) through the handset
// speaker. Idempotent — safe to call when already playing.
esp_err_t audio_start_dial_tone(void);

// Stop the dial tone and mute the DAC. Idempotent.
esp_err_t audio_stop_dial_tone(void);

// Mic test: reads from the ADC and logs peak/RMS levels every ~100 ms.
// Use to verify the analog mic path without starting a Bluetooth call.
esp_err_t audio_start_mic_test(void);
void audio_stop_mic_test(void);
bool audio_mic_test_active(void);

#endif
