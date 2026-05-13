// audio.c — ES8388 codec init via I2C, I2S setup, tone playback
//
// ES8388 at I2C address 0x10 (7-bit), pins SDA=33 SCL=32
// I2S pins: MCLK=0, BCK=27, WS=25, DOUT=26, DIN=35
// PA enable: GPIO 21 (must be HIGH for audio output)

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "audio.h"
#include "mic_source.h"

static const char *TAG = "audio";

// ---------------------------------------------------------------------------
// Hardware constants
// ---------------------------------------------------------------------------
#define ES8388_ADDR       0x10        // 7-bit I2C address
#define I2C_PORT          I2C_NUM_0
#define I2C_SDA           GPIO_NUM_33
#define I2C_SCL           GPIO_NUM_32
#define I2C_FREQ          100000

#define I2S_MCLK_PIN      GPIO_NUM_0
#define I2S_BCK_PIN       GPIO_NUM_27
#define I2S_WS_PIN        GPIO_NUM_25
#define I2S_DOUT_PIN      GPIO_NUM_26
#define I2S_DIN_PIN       GPIO_NUM_35

#define PA_ENABLE_PIN     GPIO_NUM_21

#define SAMPLE_RATE       44100
#define TONE_FREQ         440
#define TONE_AMPLITUDE    16000       // ~50% of int16 max
#define TONE_BUF_SAMPLES  256         // samples per I2S write

// ---------------------------------------------------------------------------
// I2S channel handle + call audio state
// ---------------------------------------------------------------------------
static i2s_chan_handle_t i2s_tx_handle = NULL;
static i2s_chan_handle_t i2s_rx_handle = NULL;
static RingbufHandle_t call_ringbuf = NULL;
static TaskHandle_t call_task_handle = NULL;
static bool call_active = false;

static bool dial_tone_active = false;
static TaskHandle_t dial_tone_task_handle = NULL;

static volatile bool mic_test_active = false;
static TaskHandle_t mic_test_task_handle = NULL;

#define CALL_RINGBUF_SIZE  4096   // ~250ms at 8kHz mono 16-bit
#define CALL_TASK_STACK    4096

// ---------------------------------------------------------------------------
// ES8388 I2C register read/write — uses the v6 i2c_master driver
// ---------------------------------------------------------------------------
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t es8388_dev = NULL;

static esp_err_t es8388_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(es8388_dev, buf, 2, 100);
}

__attribute__((unused))
static esp_err_t es8388_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(es8388_dev, &reg, 1, val, 1, 100);
}

// ---------------------------------------------------------------------------
// I2C bus init
// ---------------------------------------------------------------------------
static esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &i2c_bus);
    if (ret != ESP_OK) return ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8388_ADDR,
        .scl_speed_hz = I2C_FREQ,
    };
    return i2c_master_bus_add_device(i2c_bus, &dev_cfg, &es8388_dev);
}

// ---------------------------------------------------------------------------
// ES8388 codec init — DAC playback to headphone jack
// ---------------------------------------------------------------------------
static esp_err_t es8388_init(void)
{
    esp_err_t ret = ESP_OK;

    // Mute DAC first
    ret |= es8388_write_reg(0x19, 0x04);  // DACCONTROL3: mute

    // Basic chip config
    ret |= es8388_write_reg(0x01, 0x50);  // CONTROL2: standard power mgmt
    ret |= es8388_write_reg(0x02, 0x00);  // CHIPPOWER: all powered up

    // (DLL disable writes 0x35/0x37/0x39 from esp-adf reference removed
    // 2026-05-10 — they correlated with codec producing stuck DC at the I2S
    // output regardless of input. Yesterday's working binary did not have
    // these writes; restoring that state for now.)

    ret |= es8388_write_reg(0x08, 0x00);  // MASTERMODE: I2S slave (ESP32 is master)
    ret |= es8388_write_reg(0x00, 0x12);  // CONTROL1: play & record mode

    // DAC I2S format: 16-bit, I2S (Philips)
    ret |= es8388_write_reg(0x17, 0x18);  // DACCONTROL1: 16-bit I2S
    ret |= es8388_write_reg(0x18, 0x02);  // DACCONTROL2: MCLK/LRCK = 256

    // DAC mixer: route DAC to output mixer
    ret |= es8388_write_reg(0x26, 0x00);  // DACCONTROL16: LIN1/RIN1 routing
    ret |= es8388_write_reg(0x27, 0x90);  // DACCONTROL17: left DAC to mixer, 0dB
    ret |= es8388_write_reg(0x2A, 0x90);  // DACCONTROL20: right DAC to mixer, 0dB
    ret |= es8388_write_reg(0x2B, 0x80);  // DACCONTROL21: shared LRCK for ADC/DAC
    ret |= es8388_write_reg(0x2D, 0x00);  // DACCONTROL23: vroi=0

    // Output volume: LOUT1/ROUT1 — attenuate for small 8Ω handset speaker
    // 0x1E = 0dB (full), 0x1A ≈ -6dB — tweak if still buzzy/too quiet
    ret |= es8388_write_reg(0x2E, 0x1A);  // DACCONTROL24: L1 volume
    ret |= es8388_write_reg(0x2F, 0x1A);  // DACCONTROL25: R1 volume
    ret |= es8388_write_reg(0x30, 0x00);  // DACCONTROL26: L2 volume off
    ret |= es8388_write_reg(0x31, 0x00);  // DACCONTROL27: R2 volume off

    // DAC digital volume: 0dB (audio_set_volume() tweaks these at runtime)
    ret |= es8388_write_reg(0x1A, 0x00);  // DACCONTROL4: left vol = 0dB
    ret |= es8388_write_reg(0x1B, 0x00);  // DACCONTROL5: right vol = 0dB

    // Power on DAC outputs (LOUT1 + ROUT1 for handset speaker)
    ret |= es8388_write_reg(0x04, 0x3C);  // DACPOWER: enable all outputs

    // --- ADC / microphone input path ---
    // Init order matches esp-adf reference exactly (audio_hal/driver/es8388.c
    // and esp_codec_dev/device/es8388/es8388.c are identical here):
    //   1. Power DOWN ADC (0xFF) so config registers latch cleanly
    //   2. Write all ADC config registers (1-5) and digital volumes (8/9)
    //   3. Power UP ADC (0x09 = MICBIAS off, or 0x00 = MICBIAS on)
    // Previously we did the reverse (power on, then config) — this seems to
    // leave the ADC in a state that produces stuck output regardless of input
    // (DIARY 2026-05-10).
    // ADCCONTROL7 (0x0F) intentionally NOT written — esp-adf leaves it at
    // chip default. Our earlier write of 0x0A was a guess about HPF and may
    // have been putting the ADC into an unintended mode.

    ret |= es8388_write_reg(0x03, 0xFF);  // ADCPOWER: power DOWN ADC

    // 2026-05-11 night: trying the 2026-04-17 ADC values (commit 802f7d1).
    // PGA at 0x88 (datasheet-documented 24 dB max, in-spec) vs the later
    // 0xBB drift — less analog gain amplifying the codec's internal noise
    // floor, potentially better SNR even if absolute level is quieter.
    // ADCCONTROL7 = 0x20 restored from 2026-04-17 — specific bit pattern
    // hand-picked when the project was working; bit 6 differs from chip
    // default (0x60) and may control ADC soft-ramp/mute behavior.
    ret |= es8388_write_reg(0x09, 0x44);  // ADCCONTROL1: PGA gain 12 dB — sweet spot with MAX9814 (loud call confirmed 2026-05-13)
    // ADCCONTROL2 bit layout (per esp-adf reference & ES8388 datasheet):
    //   bits 7:6 LINSEL — 00=LIN1, 01=LIN2, 11=DIFF
    //   bits 5:4 RINSEL — 00=RIN1, 01=RIN2, 11=DIFF
    //   bits 3:0 DSSEL/DSR — only used in DIFF mode
    // Value 0x50 selects LIN2/RIN2. On this A541 board the "MIC1" silkscreen
    // pad routes to codec LIN2 (not LIN1) via C17; the LIN1 pin connects to
    // the LINEIN jack via a trace that is broken on A541. Verified empirically
    // 2026-05-13: 0x50 produces full mic capture on HFP call; 0x00 produces
    // stuck DC. Canonical value from esp-adf's ADC_INPUT_LINPUT2_RINPUT2 enum.
    ret |= es8388_write_reg(0x0A, 0x50);  // ADCCONTROL2: LIN2/RIN2
    ret |= es8388_write_reg(0x0B, 0x02);  // ADCCONTROL3
    ret |= es8388_write_reg(0x0C, 0x0C);  // ADCCONTROL4: 16-bit I2S
    ret |= es8388_write_reg(0x0D, 0x02);  // ADCCONTROL5: MCLK/LRCK = 256
    // ADCCONTROL7 (0x0F) NOT written — leave at chip default 0x60. See diary 2026-05-10.

    ret |= es8388_write_reg(0x10, 0x00);  // ADCCONTROL8: digital vol = 0 dB
    ret |= es8388_write_reg(0x11, 0x00);  // ADCCONTROL9: digital vol = 0 dB

    ret |= es8388_write_reg(0x0E, 0x00);  // ADCCONTROL6: ADC unmute

    ret |= es8388_write_reg(0x03, 0x00);  // ADCPOWER: power UP ADC, MICBIAS ON (keeps bias-resistor network from loading MAX9814 OUT to GND)

    // Start state machine
    ret |= es8388_write_reg(0x02, 0xF0);  // CHIPPOWER: reset state machine
    vTaskDelay(pdMS_TO_TICKS(10));
    ret |= es8388_write_reg(0x02, 0x00);  // CHIPPOWER: running

    // Unmute DAC
    ret |= es8388_write_reg(0x19, 0x00);  // DACCONTROL3: unmute

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8388 init failed");
    } else {
        ESP_LOGI(TAG, "ES8388 initialized — DAC to headphone output");
    }
    return ret;
}

// ---------------------------------------------------------------------------
// PA (power amplifier) enable — GPIO 21 HIGH
// ---------------------------------------------------------------------------
static void pa_enable(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PA_ENABLE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(PA_ENABLE_PIN, 1);
    ESP_LOGI(TAG, "PA enabled (GPIO %d HIGH)", PA_ENABLE_PIN);
}

// ---------------------------------------------------------------------------
// I2S init — standard mode, 44100Hz, 16-bit stereo
// ---------------------------------------------------------------------------
static esp_err_t i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    // Larger DMA buffers to ride through BT stack CPU bursts (mSBC decode/encode).
    // Default is 6 × 240; bumping to 12 × 320 gives ~240 ms of buffer at 16 kHz
    // stereo — eliminates the audible TX clicks (crackling on earpiece) and the
    // RX starvation events that appeared at ~33/sec during HFP calls.
    chan_cfg.dma_desc_num = 12;
    chan_cfg.dma_frame_num = 320;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx_handle, &i2s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channels");
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_PIN,
            .bclk = I2S_BCK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din  = I2S_DIN_PIN,
            .invert_flags = { 0 },
        },
    };

    ret = i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S TX standard mode");
        return ret;
    }

    ret = i2s_channel_init_std_mode(i2s_rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S RX standard mode");
        return ret;
    }

    ret = i2s_channel_enable(i2s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel");
        return ret;
    }

    ret = i2s_channel_enable(i2s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S RX channel");
        return ret;
    }

    ESP_LOGI(TAG, "I2S initialized — %dHz 16-bit stereo, TX+RX", SAMPLE_RATE);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public: init entire audio subsystem
// ---------------------------------------------------------------------------
esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "Initializing audio subsystem");

    pa_enable();

    esp_err_t ret = i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C bus ready (SDA=%d, SCL=%d)", I2C_SDA, I2C_SCL);

    ret = es8388_init();
    if (ret != ESP_OK) return ret;

    ret = i2s_init();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Audio subsystem ready");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public: play a sine wave tone for a given duration
// ---------------------------------------------------------------------------
esp_err_t audio_play_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (i2s_tx_handle == NULL) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Playing %lu Hz tone for %lu ms", freq_hz, duration_ms);

    // Stereo buffer: L, R, L, R, ...
    int16_t buf[TONE_BUF_SAMPLES * 2];
    uint32_t total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    uint32_t samples_written = 0;
    float phase = 0.0f;
    float phase_inc = 2.0f * M_PI * freq_hz / SAMPLE_RATE;

    while (samples_written < total_samples) {
        uint32_t chunk = TONE_BUF_SAMPLES;
        if (samples_written + chunk > total_samples) {
            chunk = total_samples - samples_written;
        }

        for (uint32_t i = 0; i < chunk; i++) {
            int16_t sample = (int16_t)(TONE_AMPLITUDE * sinf(phase));
            buf[i * 2]     = sample;  // left
            buf[i * 2 + 1] = sample;  // right
            phase += phase_inc;
            if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
        }

        size_t bytes_written = 0;
        i2s_channel_write(i2s_tx_handle, buf, chunk * 4, &bytes_written,
                          pdMS_TO_TICKS(1000));
        samples_written += chunk;
    }

    // Write silence to flush the DMA buffer
    memset(buf, 0, sizeof(buf));
    size_t dummy;
    i2s_channel_write(i2s_tx_handle, buf, sizeof(buf), &dummy,
                      pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Tone complete");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public: set DAC volume (0-100)
// ---------------------------------------------------------------------------
esp_err_t audio_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    // Map 0-100 to ES8388 register range: 0x00 (0dB) to 0xC0 (-96dB)
    // Higher register value = lower volume
    uint8_t reg_val = (uint8_t)((100 - volume) * 0xC0 / 100);
    esp_err_t ret = es8388_write_reg(0x1A, reg_val);
    ret |= es8388_write_reg(0x1B, reg_val);
    ESP_LOGI(TAG, "Volume set to %d%% (reg=0x%02X)", volume, reg_val);
    return ret;
}

// ---------------------------------------------------------------------------
// Public: set handset speaker volume from HFP phone-side index (0-15).
// Maps linearly to DACCONTROL24/25 in the range 0x00 (-30 dB) to 0x1A (-6 dB).
// We cap at 0x1A because the small 8 Ω handset speaker distorts above that
// (hand-tuned during the initial speaker bring-up). ES8388 register up to
// 0x1E = 0 dB; we hold 4 steps back.
// ---------------------------------------------------------------------------
esp_err_t audio_set_lout_volume(uint8_t bt_volume)
{
    if (bt_volume > 15) bt_volume = 15;
    uint8_t reg_val = (uint8_t)((bt_volume * 0x1A) / 15);
    esp_err_t ret = es8388_write_reg(0x2E, reg_val);   // DACCONTROL24: L1 volume
    ret |= es8388_write_reg(0x2F, reg_val);            // DACCONTROL25: R1 volume
    ESP_LOGI(TAG, "LOUT volume: BT=%d → reg=0x%02X", bt_volume, reg_val);
    return ret;
}

// ---------------------------------------------------------------------------
// Public: set ADC/mic PGA gain (raw register value for ADCCONTROL1)
// High nibble = left, low nibble = right, 3dB/step, 0x88 = 24dB, 0xBB = ~33dB.
// Start high for carbon mic; lower if saturating.
// ---------------------------------------------------------------------------
esp_err_t audio_set_mic_gain(uint8_t gain_reg)
{
    esp_err_t ret = es8388_write_reg(0x09, gain_reg);
    ESP_LOGI(TAG, "Mic PGA gain set (ADCCONTROL1=0x%02X)", gain_reg);
    return ret;
}

// ---------------------------------------------------------------------------
// Public: mute/unmute DAC
// ---------------------------------------------------------------------------
esp_err_t audio_set_mute(bool mute)
{
    ESP_LOGI(TAG, "DAC %s", mute ? "muted" : "unmuted");
    return es8388_write_reg(0x19, mute ? 0x04 : 0x00);
}

// ---------------------------------------------------------------------------
// Call audio playback task — reads mono PCM from ring buffer,
// duplicates to stereo, writes to I2S
// ---------------------------------------------------------------------------
static void call_audio_task(void *arg)
{
    ESP_LOGI(TAG, "Call audio task started");
    int16_t stereo_buf[256];  // 128 stereo samples

    while (call_active) {
        size_t item_size = 0;
        void *item = xRingbufferReceiveUpTo(call_ringbuf, &item_size,
                                             pdMS_TO_TICKS(50), 256);
        if (item == NULL || item_size == 0) {
            continue;
        }

        // item contains mono 16-bit PCM samples
        const int16_t *mono = (const int16_t *)item;
        size_t mono_samples = item_size / 2;

        // Convert mono to stereo (duplicate each sample)
        for (size_t i = 0; i < mono_samples && i < 128; i++) {
            stereo_buf[i * 2]     = mono[i];
            stereo_buf[i * 2 + 1] = mono[i];
        }

        vRingbufferReturnItem(call_ringbuf, item);

        size_t bytes_written = 0;
        i2s_channel_write(i2s_tx_handle, stereo_buf,
                          mono_samples * 4,  // stereo 16-bit = 4 bytes per sample
                          &bytes_written, pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Call audio task ended");
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Shared I2S RX handle — read by mic_source_codec.c during call audio
// ---------------------------------------------------------------------------
i2s_chan_handle_t audio_get_i2s_rx_handle(void)
{
    return i2s_rx_handle;
}

// ---------------------------------------------------------------------------
// Mic test task — reads raw ADC samples (left channel) and logs peak + RMS
// every ~100 ms. Use to confirm the handset mic reaches the codec without
// going through the Bluetooth call path.
// ---------------------------------------------------------------------------
static void mic_test_task(void *arg)
{
    ESP_LOGI(TAG, "Mic test: tap / speak into the handset");
    int16_t buf[256];   // 128 stereo frames per read

    int32_t peak = 0;
    int64_t sum_sq = 0;
    int64_t n = 0;
    int64_t last_log = esp_timer_get_time();

    while (mic_test_active) {
        size_t bytes_read = 0;
        esp_err_t r = i2s_channel_read(i2s_rx_handle, buf, sizeof(buf),
                                       &bytes_read, pdMS_TO_TICKS(100));
        if (r != ESP_OK || bytes_read == 0) continue;

        size_t stereo_frames = bytes_read / 4;
        for (size_t i = 0; i < stereo_frames; i++) {
            int16_t s = buf[i * 2];             // left channel = mic
            int32_t a = s < 0 ? -s : s;
            if (a > peak) peak = a;
            sum_sq += (int32_t)s * s;
            n++;
        }

        int64_t now = esp_timer_get_time();
        if (now - last_log >= 100000 && n > 0) {
            int32_t rms = (int32_t)sqrt((double)sum_sq / (double)n);
            ESP_LOGI(TAG, "mic peak=%5ld rms=%5ld  (%lld samples)",
                     (long)peak, (long)rms, n);
            peak = 0;
            sum_sq = 0;
            n = 0;
            last_log = now;
        }
    }

    ESP_LOGI(TAG, "Mic test stopped");
    vTaskDelete(NULL);
}

esp_err_t audio_start_mic_test(void)
{
    if (mic_test_active) return ESP_OK;
    if (call_active) {
        ESP_LOGW(TAG, "Refusing to start mic test during call");
        return ESP_ERR_INVALID_STATE;
    }
    mic_test_active = true;
    xTaskCreate(mic_test_task, "mic_test", CALL_TASK_STACK, NULL, 4,
                &mic_test_task_handle);
    return ESP_OK;
}

void audio_stop_mic_test(void)
{
    if (!mic_test_active) return;
    mic_test_active = false;
    mic_test_task_handle = NULL;
}

bool audio_mic_test_active(void)
{
    return mic_test_active;
}

// ---------------------------------------------------------------------------
// Public: start call audio mode
// ---------------------------------------------------------------------------
esp_err_t audio_start_call(uint32_t sample_rate)
{
    if (call_active) return ESP_OK;

    ESP_LOGI(TAG, "Starting call audio at %lu Hz", sample_rate);

    // Stop dial tone first — its task writes to i2s_tx_handle, and we're about
    // to disable/reconfigure that channel. Letting them race produces thousands
    // of "channel is not enabled" errors and corrupts I2S state.
    audio_stop_dial_tone();

    // Reconfigure I2S TX+RX clock for voice sample rate
    i2s_channel_disable(i2s_tx_handle);
    i2s_channel_disable(i2s_rx_handle);

    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    esp_err_t ret = i2s_channel_reconfig_std_clock(i2s_tx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfig TX clock: %s", esp_err_to_name(ret));
        i2s_channel_enable(i2s_tx_handle);
        i2s_channel_enable(i2s_rx_handle);
        return ret;
    }
    ret = i2s_channel_reconfig_std_clock(i2s_rx_handle, &clk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reconfig RX clock: %s", esp_err_to_name(ret));
    }

    i2s_channel_enable(i2s_tx_handle);
    i2s_channel_enable(i2s_rx_handle);

    // Speaker side ring buffer (BT → I2S)
    call_ringbuf = xRingbufferCreate(CALL_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (call_ringbuf == NULL) {
        ESP_LOGE(TAG, "Failed to create call ring buffer");
        return ESP_ERR_NO_MEM;
    }

    // Unmute DAC, start speaker playback task
    audio_set_mute(false);
    call_active = true;
    xTaskCreate(call_audio_task, "call_audio", CALL_TASK_STACK, NULL, 5,
                &call_task_handle);

    // Mic side: hand off to whichever source is active
    esp_err_t mret = mic_source_start(sample_rate);
    if (mret != ESP_OK) {
        ESP_LOGW(TAG, "mic_source_start failed: %s", esp_err_to_name(mret));
    }

    ESP_LOGI(TAG, "Call audio started (TX+RX active)");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public: stop call audio mode
// ---------------------------------------------------------------------------
esp_err_t audio_stop_call(void)
{
    if (!call_active) return ESP_OK;

    ESP_LOGI(TAG, "Stopping call audio");

    // Stop mic side first (its task reads from the same I2S RX we're about
    // to disable below)
    mic_source_stop();

    // Signal speaker task to stop
    call_active = false;
    vTaskDelay(pdMS_TO_TICKS(150));   // let task observe flag and exit
    call_task_handle = NULL;

    // Mute DAC
    audio_set_mute(true);

    // Free speaker ring buffer
    if (call_ringbuf) {
        vRingbufferDelete(call_ringbuf);
        call_ringbuf = NULL;
    }

    // Reconfigure I2S TX+RX back to 44100Hz for tones
    i2s_channel_disable(i2s_tx_handle);
    i2s_channel_disable(i2s_rx_handle);
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE);
    i2s_channel_reconfig_std_clock(i2s_tx_handle, &clk_cfg);
    i2s_channel_reconfig_std_clock(i2s_rx_handle, &clk_cfg);
    i2s_channel_enable(i2s_tx_handle);
    i2s_channel_enable(i2s_rx_handle);

    ESP_LOGI(TAG, "Call audio stopped, I2S back to %d Hz", SAMPLE_RATE);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public: write incoming call audio (called from BT task, non-blocking)
// ---------------------------------------------------------------------------
void audio_write_call_data(const uint8_t *data, uint32_t len)
{
    if (!call_active || call_ringbuf == NULL || len == 0) return;

    // Non-blocking write — drop data if buffer full (better than blocking BT task)
    xRingbufferSend(call_ringbuf, data, len, 0);
}

// ---------------------------------------------------------------------------
// Dial tone — 350 + 440 Hz sine mix, looped until stopped
// ---------------------------------------------------------------------------
static void dial_tone_task(void *arg)
{
    ESP_LOGI(TAG, "Dial tone task started");
    int16_t buf[TONE_BUF_SAMPLES * 2];
    float phase1 = 0.0f, phase2 = 0.0f;
    float inc1 = 2.0f * M_PI * 350 / SAMPLE_RATE;
    float inc2 = 2.0f * M_PI * 440 / SAMPLE_RATE;

    while (dial_tone_active) {
        for (uint32_t i = 0; i < TONE_BUF_SAMPLES; i++) {
            int16_t s = (int16_t)((sinf(phase1) + sinf(phase2)) * (TONE_AMPLITUDE / 2));
            buf[i * 2]     = s;
            buf[i * 2 + 1] = s;
            phase1 += inc1; if (phase1 >= 2.0f * M_PI) phase1 -= 2.0f * M_PI;
            phase2 += inc2; if (phase2 >= 2.0f * M_PI) phase2 -= 2.0f * M_PI;
        }
        size_t written = 0;
        i2s_channel_write(i2s_tx_handle, buf, sizeof(buf), &written,
                          pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Dial tone task ended");
    vTaskDelete(NULL);
}

esp_err_t audio_start_dial_tone(void)
{
    if (dial_tone_active) return ESP_OK;
    if (call_active) {
        ESP_LOGW(TAG, "Refusing to start dial tone during call");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting dial tone");
    audio_set_mute(false);
    dial_tone_active = true;
    xTaskCreate(dial_tone_task, "dial_tone", 4096, NULL, 5, &dial_tone_task_handle);
    return ESP_OK;
}

esp_err_t audio_stop_dial_tone(void)
{
    if (!dial_tone_active) return ESP_OK;

    ESP_LOGI(TAG, "Stopping dial tone");
    dial_tone_active = false;
    vTaskDelay(pdMS_TO_TICKS(150));   // let task observe the flag and exit
    dial_tone_task_handle = NULL;
    audio_set_mute(true);
    return ESP_OK;
}
