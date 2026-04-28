// mic_source.c — dispatcher

#include <string.h>
#include "esp_log.h"
#include "mic_source.h"

static const char *TAG = "mic_source";

static const mic_source_t *active = NULL;

void mic_source_set(const mic_source_t *src)
{
    active = src;
    ESP_LOGI(TAG, "active source: %s", src ? src->name : "(none)");
}

esp_err_t mic_source_start(uint32_t sample_rate)
{
    if (active == NULL || active->start == NULL) return ESP_OK;
    return active->start(sample_rate);
}

esp_err_t mic_source_stop(void)
{
    if (active == NULL || active->stop == NULL) return ESP_OK;
    return active->stop();
}

uint32_t mic_source_read(uint8_t *buf, uint32_t len)
{
    if (active == NULL || active->read == NULL) {
        if (buf) memset(buf, 0, len);
        return len;
    }
    return active->read(buf, len);
}
