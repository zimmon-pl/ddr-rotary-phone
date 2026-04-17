// bluetooth.c — HFP Hands-Free client
//
// Sets up Bluetooth Classic, registers as HFP HF device named "FestStefan",
// and waits for a phone to connect. Logs all HFP events to serial.

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_hf_client_legacy_api.h"
#include "bluetooth.h"
#include "audio.h"

static const char *TAG = "bluetooth";

#define BT_DEVICE_NAME "FestStefan"

// Call state — mirrored from HFP CIND indicators
static bool is_ringing = false;
static bool is_in_call = false;

// ---------------------------------------------------------------------------
// GAP callback — handles pairing, authentication, discovery
// ---------------------------------------------------------------------------
static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Authentication success: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "Authentication failed, status: %d", param->auth_cmpl.stat);
        }
        break;

    case ESP_BT_GAP_PIN_REQ_EVT:
        ESP_LOGI(TAG, "PIN requested, replying 0000");
        esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
        break;

    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(TAG, "SSP confirm request, numeric value: %06"PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;

    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "SSP passkey: %06"PRIu32, param->key_notif.passkey);
        break;

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(TAG, "Power mode changed: %d", param->mode_chg.mode);
        break;

    default:
        ESP_LOGD(TAG, "GAP event: %d", event);
        break;
    }
}

// ---------------------------------------------------------------------------
// HFP audio data callbacks (Voice over HCI)
// ---------------------------------------------------------------------------
static void hf_incoming_data_cb(const uint8_t *buf, uint32_t len)
{
    // Decoded PCM from BT stack → ring buffer → I2S → headphones
    audio_write_call_data(buf, len);
}

static uint32_t hf_outgoing_data_cb(uint8_t *buf, uint32_t len)
{
    // Handset mic PCM from I2S RX → BT stack (zero-pads on underflow)
    return audio_read_mic_data(buf, len);
}

// ---------------------------------------------------------------------------
// HFP client callback — handles connection, call, and audio events
// ---------------------------------------------------------------------------
static void hf_client_callback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    switch (event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT: {
        const char *states[] = {"disconnected", "connecting", "connected", "slc_connected", "disconnecting"};
        ESP_LOGI(TAG, "Connection: %s", states[param->conn_stat.state]);
        if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED) {
            ESP_LOGI(TAG, "Service Level Connection established — ready for calls");
        }
        break;
    }

    case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
        const char *states[] = {"disconnected", "connecting", "connected", "connected_msbc"};
        ESP_LOGI(TAG, "Audio: %s", states[param->audio_stat.state]);

        if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED) {
            // CVSD codec: 8kHz
            audio_start_call(8000);
        } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
            // mSBC (Wide Band Speech): 16kHz
            audio_start_call(16000);
        } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
            audio_stop_call();
        }
        break;
    }

    case ESP_HF_CLIENT_CIND_CALL_EVT:
        is_in_call = (param->call.status != 0);
        ESP_LOGI(TAG, "Call indicator: %s",
                 is_in_call ? "CALL IN PROGRESS" : "no call");
        break;

    case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT: {
        const char *setup[] = {"none", "INCOMING", "outgoing_dialing", "outgoing_alerting"};
        ESP_LOGI(TAG, "Call setup: %s", setup[param->call_setup.status]);
        is_ringing = (param->call_setup.status == 1);  // 1 = incoming
        break;
    }

    case ESP_HF_CLIENT_RING_IND_EVT:
        ESP_LOGI(TAG, "*** RING! Incoming call ***");
        break;

    case ESP_HF_CLIENT_CLIP_EVT:
        ESP_LOGI(TAG, "Caller ID: %s",
                 param->clip.number ? param->clip.number : "unknown");
        break;

    case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
        ESP_LOGI(TAG, "Volume %s: %d",
                 param->volume_control.type == ESP_HF_VOLUME_CONTROL_TARGET_SPK ? "speaker" : "mic",
                 param->volume_control.volume);
        break;

    case ESP_HF_CLIENT_CIND_SERVICE_AVAILABILITY_EVT:
        ESP_LOGI(TAG, "Network: %s",
                 param->service_availability.status ? "available" : "unavailable");
        break;

    case ESP_HF_CLIENT_CIND_SIGNAL_STRENGTH_EVT:
        ESP_LOGI(TAG, "Signal strength: %d", param->signal_strength.value);
        break;

    case ESP_HF_CLIENT_CIND_BATTERY_LEVEL_EVT:
        ESP_LOGI(TAG, "Battery level: %d", param->battery_level.value);
        break;

    case ESP_HF_CLIENT_BSIR_EVT:
        ESP_LOGI(TAG, "In-band ring tone: %s",
                 param->bsir.state ? "provided" : "not provided");
        break;

    case ESP_HF_CLIENT_PROF_STATE_EVT:
        if (param->prof_stat.state == ESP_HF_INIT_SUCCESS) {
            ESP_LOGI(TAG, "HFP profile initialized");
        }
        break;

    default:
        ESP_LOGD(TAG, "HFP event: %d", event);
        break;
    }
}

// ---------------------------------------------------------------------------
// BT stack ready — set name, register profiles, make discoverable
// ---------------------------------------------------------------------------
static void bt_stack_ready(void)
{
    esp_bt_gap_set_device_name(BT_DEVICE_NAME);
    ESP_LOGI(TAG, "Device name set: %s", BT_DEVICE_NAME);

    esp_bt_gap_register_callback(gap_callback);
    esp_hf_client_register_callback(hf_client_callback);
    esp_hf_client_init();

    // Register audio data callbacks for Voice over HCI
    esp_hf_client_register_data_callback(hf_incoming_data_cb, hf_outgoing_data_cb);

    // SSP (Secure Simple Pairing) — "just works" mode
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    // Also support legacy PIN pairing
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code = {'0', '0', '0', '0'};
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    // Make discoverable and connectable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    ESP_LOGI(TAG, "Discoverable — waiting for phone to connect...");
}

// ---------------------------------------------------------------------------
// Public: init entire Bluetooth subsystem
// ---------------------------------------------------------------------------
esp_err_t bluetooth_init(void)
{
    ESP_LOGI(TAG, "Initializing Bluetooth");

    // NVS is required for BT (stores calibration + bonding data)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Release BLE memory — we only use Classic BT
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    // Init and enable BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Init and enable Bluedroid
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Log our BT address
    const uint8_t *addr = esp_bt_dev_get_address();
    ESP_LOGI(TAG, "BT address: %02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // Set up HFP and make discoverable
    bt_stack_ready();

    ESP_LOGI(TAG, "Bluetooth ready");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Public: call-control helpers driven by the hook switch
// ---------------------------------------------------------------------------
esp_err_t bluetooth_answer_call(void)
{
    ESP_LOGI(TAG, "Answering call");
    return esp_hf_client_answer_call();
}

esp_err_t bluetooth_hangup_call(void)
{
    ESP_LOGI(TAG, "Hanging up");
    return esp_hf_client_reject_call();
}

bool bluetooth_is_ringing(void) { return is_ringing; }
bool bluetooth_is_in_call(void) { return is_in_call; }
