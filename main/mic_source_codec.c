// mic_source_codec.c — mic input via the ES8388 codec on I2S RX
//
// This is the path FestStefan has shipped with so far: ES8388 ADC samples a
// stereo I2S frame, we drop the right channel, push mono PCM into a ring
// buffer, and the HFP outgoing-data callback drains it.
//
// On the A1S V2.2 board this path is muted by a hardware short between LINEIN
// and the onboard MEMS mics (DIARY 2026-04-24). The code here is correct; the
// board is the problem. mic_source_adc1 will be the alternative.

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

static void mic_capture_task(void *arg)
{
    ESP_LOGI(TAG, "capture task started");
    int16_t stereo_buf[I2S_READ_FRAMES * 2];   // 256 stereo frames = 1024 bytes
    int16_t mono_buf[I2S_READ_FRAMES];         // 256 mono samples = 512 bytes

    i2s_chan_handle_t rx = audio_get_i2s_rx_handle();

    while (capture_active) {
        size_t bytes_read = 0;
        esp_err_t r = i2s_channel_read(rx, stereo_buf, sizeof(stereo_buf),
                                       &bytes_read, pdMS_TO_TICKS(100));
        if (r != ESP_OK || bytes_read == 0) continue;

        size_t stereo_samples = bytes_read / 4;
        if (stereo_samples > I2S_READ_FRAMES) stereo_samples = I2S_READ_FRAMES;

        for (size_t i = 0; i < stereo_samples; i++) {
            mono_buf[i] = stereo_buf[i * 2];   // drop right channel (MIC2 is dead)
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
