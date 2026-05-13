// mic_source_codec.c — mic input via the ES8388 codec on I2S RX
//
// This is the path FestStefan has shipped with so far: ES8388 ADC samples a
// stereo I2S frame, we drop the right channel, push mono PCM into a ring
// buffer, and the HFP outgoing-data callback drains it.
//
// On A541 the LINEIN-jack→codec trace is broken (DIARY 2026-05-06). Working
// around it by feeding the bias tee into the desoldered MIC1 footprint,
// reaching the ADC via the original MIC1→C17→LIN1 path. ADCCONTROL2 is
// configured for LIN1/RIN1 in audio.c.

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "audio.h"
#include "bluetooth.h"
#include "mic_source.h"

static const char *TAG = "mic_codec";

#define MIC_RINGBUF_SIZE  12288  // ~380 ms at 16 kHz mono 16-bit
#define I2S_READ_FRAMES   256    // 256 stereo frames per read = 16 ms @ 16 kHz
#define PREBUFFER_PUSHES  3      // buffer ~3 KB before nudging BT for the first time
#define MIC_TASK_STACK    4096

// Software gain on captured PCM.
#define MIC_GAIN_SHIFT 0   // 1× (no software gain). MAX9814 + codec PGA 12 dB carry all the gain; software boost was clipping mSBC encoder.

// Standard one-pole DC blocker:  y[n] = x[n] - x[n-1] + R * y[n-1].
// Required: without it every sample saturates to INT16_MAX after the gain
// stage because the MICBIAS path leaves a large DC offset that the codec's
// own HPF doesn't fully remove. R = 32700/32768 ≈ 0.998 → ~5 Hz cutoff.
#define DC_BLOCK_R_NUM 32700
#define DC_BLOCK_R_DEN 32768

static RingbufHandle_t mic_ringbuf = NULL;
static TaskHandle_t mic_task_handle = NULL;
static volatile bool capture_active = false;

// Debug instrumentation — prove (or disprove) that bytes are flowing from our
// ring buffer to the BT stack during a call.
static uint32_t bt_read_calls = 0;
static uint32_t bt_underflows = 0;
static uint32_t bt_bytes_sent = 0;

// Capture-side state: prebuffer counter, gates the first BT notification
// until we've pushed enough chunks to keep up with the BT pull rate.
static int prebuffer_count = 0;

// DC-blocker state. File-scope (not static-in-function) so we can reset it
// from codec_start; otherwise filter state from the previous call leaks into
// the new call and produces a brief transient at startup.
static int16_t dc_prev_x = 0;
static int32_t dc_prev_y = 0;

static void mic_capture_task(void *arg)
{
    ESP_LOGI(TAG, "capture task started");
    int16_t stereo_buf[I2S_READ_FRAMES * 2];   // 256 stereo frames = 1024 bytes
    int16_t mono_buf[I2S_READ_FRAMES];         // 256 mono samples = 512 bytes

    i2s_chan_handle_t rx = audio_get_i2s_rx_handle();

    uint32_t i2s_starve_count = 0;
    while (capture_active) {
        size_t bytes_read = 0;
        esp_err_t r = i2s_channel_read(rx, stereo_buf, sizeof(stereo_buf),
                                       &bytes_read, pdMS_TO_TICKS(100));
        // Partial fill (ESP_ERR_TIMEOUT with bytes_read > 0) is normal — the
        // BT stack pulls in 240-byte mSBC frames so I2S DMA often hands us
        // less than a full requested chunk. Just process whatever we got.
        if (bytes_read == 0) {
            // Real starvation — codec/I2S not producing anything. Rare.
            if (i2s_starve_count == 0 || (i2s_starve_count % 200) == 0) {
                ESP_LOGW(TAG, "I2S RX starved err=%s (count=%lu)",
                         esp_err_to_name(r), i2s_starve_count);
            }
            i2s_starve_count++;
            continue;
        }

        size_t stereo_samples = bytes_read / 4;
        if (stereo_samples > I2S_READ_FRAMES) stereo_samples = I2S_READ_FRAMES;

        for (size_t i = 0; i < stereo_samples; i++) {
            int16_t x = stereo_buf[i * 2];     // drop right channel
            int32_t y = ((int32_t)x - dc_prev_x) +
                        (dc_prev_y * DC_BLOCK_R_NUM / DC_BLOCK_R_DEN);
            dc_prev_x = x;
            dc_prev_y = y;

            int32_t scaled = y << MIC_GAIN_SHIFT;
            if (scaled > INT16_MAX) scaled = INT16_MAX;
            else if (scaled < INT16_MIN) scaled = INT16_MIN;
            mono_buf[i] = (int16_t)scaled;
        }

        // Non-blocking — drop samples if BT isn't draining (better than stalling)
        xRingbufferSend(mic_ringbuf, mono_buf, stereo_samples * 2, 0);

        // First few pushes: stay quiet so the buffer accumulates a head-start
        // before BT begins pulling.
        if (prebuffer_count < PREBUFFER_PUSHES) {
            prebuffer_count++;
            continue;
        }

        // Push-notify the BT stack: without this it never pulls outgoing audio.
        bluetooth_notify_outgoing_ready();
    }

    ESP_LOGI(TAG, "capture task ended");
    vTaskDelete(NULL);
}

static esp_err_t codec_start(uint32_t sample_rate)
{
    (void)sample_rate;   // I2S rate is configured by audio_start_call
    if (capture_active) return ESP_OK;

    mic_ringbuf = xRingbufferCreate(MIC_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (mic_ringbuf == NULL) {
        ESP_LOGE(TAG, "ringbuf alloc failed");
        return ESP_ERR_NO_MEM;
    }

    bt_read_calls    = 0;
    bt_underflows    = 0;
    bt_bytes_sent    = 0;
    prebuffer_count  = 0;
    dc_prev_x        = 0;
    dc_prev_y        = 0;

    capture_active = true;
    xTaskCreate(mic_capture_task, "mic_capture", MIC_TASK_STACK, NULL, 5,
                &mic_task_handle);
    return ESP_OK;
}

static esp_err_t codec_stop(void)
{
    if (!capture_active) return ESP_OK;

    ESP_LOGI(TAG, "session stats: BT pulled %lu times, %lu underflows, "
                  "%lu bytes sent",
             bt_read_calls, bt_underflows, bt_bytes_sent);

    capture_active = false;
    vTaskDelay(pdMS_TO_TICKS(150));   // let task observe flag and exit
    mic_task_handle = NULL;

    if (mic_ringbuf) {
        vRingbufferDelete(mic_ringbuf);
        mic_ringbuf = NULL;
    }
    return ESP_OK;
}

static uint32_t codec_read(uint8_t *buf, uint32_t len)
{
    bt_read_calls++;
    if (bt_read_calls == 1) {
        ESP_LOGI(TAG, "first BT outgoing read: len=%lu", len);
    }

    if (!capture_active || mic_ringbuf == NULL || len == 0) {
        if (buf) memset(buf, 0, len);
        bt_underflows++;
        return len;
    }

    uint32_t filled = 0;
    while (filled < len) {
        size_t item_size = 0;
        void *item = xRingbufferReceiveUpTo(mic_ringbuf, &item_size,
                                            0, len - filled);
        if (item == NULL || item_size == 0) break;
        memcpy(buf + filled, item, item_size);
        vRingbufferReturnItem(mic_ringbuf, item);
        filled += item_size;
    }

    if (filled < len) {
        memset(buf + filled, 0, len - filled);
        bt_underflows++;
    }
    bt_bytes_sent += filled;

    // Every ~250 frames (≈1 s at 16 kHz mSBC) log peak from current frame so
    // we can tell whether the bytes we're handing to BT are silence or audio.
    if (bt_read_calls % 250 == 0 && filled > 0) {
        const int16_t *s = (const int16_t *)buf;
        size_t n = filled / 2;
        int32_t peak = 0;
        for (size_t i = 0; i < n; i++) {
            int32_t a = s[i] < 0 ? -s[i] : s[i];
            if (a > peak) peak = a;
        }
        ESP_LOGI(TAG, "BT frame #%lu  peak=%5ld  filled=%lu/%lu  underflows=%lu",
                 bt_read_calls, (long)peak, filled, len, bt_underflows);
    }

    return len;
}

const mic_source_t mic_source_codec = {
    .name  = "codec",
    .start = codec_start,
    .stop  = codec_stop,
    .read  = codec_read,
};
