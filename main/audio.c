// audio.c — ES8388 codec init via I2C, I2S setup, tone playback
//
// ES8388 at I2C address 0x10 (7-bit), pins SDA=33 SCL=32
// I2S pins: MCLK=0, BCK=27, WS=25, DOUT=26, DIN=35
// PA enable: GPIO 21 (must be HIGH for audio output)

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "audio.h"

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
// I2S channel handle
// ---------------------------------------------------------------------------
static i2s_chan_handle_t i2s_tx_handle = NULL;

// ---------------------------------------------------------------------------
// ES8388 I2C register read/write
// ---------------------------------------------------------------------------
static esp_err_t es8388_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(I2C_PORT, ES8388_ADDR, buf, 2,
                                      pdMS_TO_TICKS(100));
}

static esp_err_t es8388_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(I2C_PORT, ES8388_ADDR,
                                        &reg, 1, val, 1,
                                        pdMS_TO_TICKS(100));
}

// ---------------------------------------------------------------------------
// I2C bus init
// ---------------------------------------------------------------------------
static esp_err_t i2c_bus_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) return ret;
    return i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
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

    // Output volume: LOUT1/ROUT1 (headphone) at 0dB
    ret |= es8388_write_reg(0x2E, 0x1E);  // DACCONTROL24: L1 volume = 0dB
    ret |= es8388_write_reg(0x2F, 0x1E);  // DACCONTROL25: R1 volume = 0dB
    ret |= es8388_write_reg(0x30, 0x00);  // DACCONTROL26: L2 volume off
    ret |= es8388_write_reg(0x31, 0x00);  // DACCONTROL27: R2 volume off

    // DAC digital volume: 0dB
    ret |= es8388_write_reg(0x1A, 0x00);  // DACCONTROL4: left vol = 0dB
    ret |= es8388_write_reg(0x1B, 0x00);  // DACCONTROL5: right vol = 0dB

    // Power on DAC outputs (LOUT1 + ROUT1 for headphone)
    ret |= es8388_write_reg(0x04, 0x3C);  // DACPOWER: enable all outputs

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
    esp_err_t ret = i2s_new_channel(&chan_cfg, &i2s_tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel");
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
        ESP_LOGE(TAG, "Failed to init I2S standard mode");
        return ret;
    }

    ret = i2s_channel_enable(i2s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel");
        return ret;
    }

    ESP_LOGI(TAG, "I2S initialized — %dHz, 16-bit stereo", SAMPLE_RATE);
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
// Public: mute/unmute DAC
// ---------------------------------------------------------------------------
esp_err_t audio_set_mute(bool mute)
{
    ESP_LOGI(TAG, "DAC %s", mute ? "muted" : "unmuted");
    return es8388_write_reg(0x19, mute ? 0x04 : 0x00);
}
