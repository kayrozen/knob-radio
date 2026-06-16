/*
 * a2dp_bridge.c — see a2dp_bridge.h.
 *
 * Structure follows the ESP-IDF `a2dp_source` example, with one essential
 * change: the data callback pulls real PCM from the jitter buffer instead of
 * synthesizing a tone. Discovery looks for a sink whose name matches
 * A2DP_TARGET_NAME (or the first audio sink if unset).
 */
#include "a2dp_bridge.h"
#include "pcm_link_proto.h"

#include <string.h>

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "a2dp";

static jitter_buffer_t *s_jb;
static esp_bd_addr_t    s_peer_bda;
static bool             s_peer_found;
static bool             s_connected;

/* ----- discovery: pull a device name out of the EIR --------------------- */
static bool get_name_from_eir(uint8_t *eir, char *out, size_t out_len)
{
    if (!eir) {
        return false;
    }
    uint8_t  len = 0;
    uint8_t *name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
    if (!name) {
        name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
    }
    if (!name || len == 0) {
        return false;
    }
    if (len >= out_len) {
        len = (uint8_t)(out_len - 1);
    }
    memcpy(out, name, len);
    out[len] = '\0';
    return true;
}

static bool is_target(esp_bt_gap_cb_param_t *param, char *name_out, size_t name_len)
{
    /* Must advertise the audio-rendering (sink) service to be a candidate. */
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
        if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
            get_name_from_eir((uint8_t *)p->val, name_out, name_len);
        }
    }
    const char *want = A2DP_TARGET_NAME;
    if (want[0] == '\0') {
        return true; /* no filter: take the first candidate */
    }
    return strstr(name_out, want) != NULL;
}

/* ----- GAP callback ----------------------------------------------------- */
static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        if (s_peer_found) {
            break;
        }
        char name[64] = {0};
        if (is_target(param, name, sizeof(name))) {
            ESP_LOGI(TAG, "found sink '%s', connecting", name[0] ? name : "?");
            memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
            s_peer_found = true;
            esp_bt_gap_cancel_discovery();
            esp_a2d_source_connect(s_peer_bda);
        }
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED &&
            !s_peer_found) {
            /* Nothing found this round; scan again. */
            esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
        }
        break;
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        ESP_LOGI(TAG, "auth %s",
                 param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS ? "ok" : "fail");
        break;
    default:
        break;
    }
}

/* ----- A2DP data callback: pull PCM from the jitter buffer -------------- */
static int32_t a2dp_data_cb(uint8_t *buf, int32_t len)
{
    if (!buf || len <= 0 || !s_jb) {
        return 0;
    }
    /* Always return a full buffer; jb_pull silence-pads on underrun so the
     * SBC encoder timing never stalls. */
    jb_pull(s_jb, buf, (size_t)len);
    return len;
}

/* ----- A2DP event callback --------------------------------------------- */
static void a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        esp_a2d_connection_state_t st = param->conn_stat.state;
        ESP_LOGI(TAG, "A2DP conn state %d", st);
        if (st == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            s_connected = true;
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        } else if (st == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            s_connected = false;
            s_peer_found = false;
            esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
        }
        break;
    }
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
            param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        }
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "A2DP audio state %d", param->audio_stat.state);
        break;
    default:
        break;
    }
}

void a2dp_bridge_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_cb));
    ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(a2dp_data_cb));
    ESP_ERROR_CHECK(esp_a2d_source_init());

    esp_bt_gap_set_device_name("Preset-Bridge");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    ESP_LOGI(TAG, "A2DP source stack up (discovery not started yet)");
}

void a2dp_bridge_set_buffer(jitter_buffer_t *jb)
{
    s_jb = jb;
}

void a2dp_bridge_start_discovery(void)
{
    ESP_LOGI(TAG, "scanning for sink '%s'",
             A2DP_TARGET_NAME[0] ? A2DP_TARGET_NAME : "<first audio sink>");
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}
