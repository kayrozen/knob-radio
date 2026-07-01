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
#include "link_tx.h"

#include <string.h>

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "a2dp";

static jitter_buffer_t *s_jb;
static esp_bd_addr_t    s_peer_bda;
static bool             s_peer_found;
static bool             s_connected;
static bool             s_scanning;       /* report sinks instead of connecting */
static char             s_metadata[64];   /* now-playing title (for AVRCP TG) */

static void send_bt_status(uint8_t state)
{
    link_tx_send_control(PCM_LINK_CTRL_BT_STATUS, &state, 1);
}

void a2dp_bridge_report_status(void)
{
    send_bt_status(s_connected ? PCM_LINK_BT_CONNECTED : PCM_LINK_BT_DISCONNECTED);
}

void a2dp_bridge_set_metadata(const char *title)
{
    if (!title) {
        return;
    }
    strncpy(s_metadata, title, sizeof(s_metadata) - 1);
    s_metadata[sizeof(s_metadata) - 1] = '\0';
    /* Phase 11 will surface this as the AVRCP target's TITLE attribute. */
    ESP_LOGI(TAG, "now playing: %s", s_metadata);
}

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

/* True when the Class of Device says this is an audio sink (a car head unit,
 * speaker, headphones): Audio/Video major class AND an audio/rendering service
 * bit. Requiring the AV major class keeps out non-audio devices that also set
 * the Rendering bit (e.g. printers, major class Imaging). Phones/laptops fail. */
static bool cod_is_audio_sink(uint32_t cod)
{
    if (!esp_bt_gap_is_valid_cod(cod)) {
        return false;
    }
    if (esp_bt_gap_get_cod_major_dev(cod) != ESP_BT_COD_MAJOR_DEV_AV) {
        return false;
    }
    uint32_t srvc = esp_bt_gap_get_cod_srvc(cod);
    return (srvc & ESP_BT_COD_SRVC_RENDERING) || (srvc & ESP_BT_COD_SRVC_AUDIO);
}

/* Safely read a COD property value: it may be NULL, the wrong length, or
 * unaligned, so validate and memcpy rather than dereferencing a cast pointer. */
static bool prop_is_audio_sink(const esp_bt_gap_dev_prop_t *p)
{
    if (!p->val || p->len != sizeof(uint32_t)) {
        return false;
    }
    uint32_t cod = 0;
    memcpy(&cod, p->val, sizeof(cod));
    return cod_is_audio_sink(cod);
}

static bool is_target(esp_bt_gap_cb_param_t *param, char *name_out, size_t name_len)
{
    /* Must look like an audio sink to be a candidate at all — without this,
     * the no-name-filter default would connect to the first device discovered
     * (a phone, a laptop, ...). */
    bool sink = false;
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
        if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
            get_name_from_eir((uint8_t *)p->val, name_out, name_len);
        } else if (p->type == ESP_BT_GAP_DEV_PROP_COD) {
            sink = prop_is_audio_sink(p);
        }
    }
    if (!sink) {
        return false;
    }
    const char *want = A2DP_TARGET_NAME;
    if (want[0] == '\0') {
        return true; /* no name filter: take the first audio sink */
    }
    return strstr(name_out, want) != NULL;
}

/* ----- GAP callback ----------------------------------------------------- */
static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        char name[64] = {0};
        /* Orchestrated scan: relay every discovered device to the S3 (which
         * shows the list in the portal) and do not auto-connect. */
        if (s_scanning) {
            bool sink = false;
            for (int i = 0; i < param->disc_res.num_prop; i++) {
                esp_bt_gap_dev_prop_t *p = &param->disc_res.prop[i];
                if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
                    get_name_from_eir((uint8_t *)p->val, name, sizeof(name));
                } else if (p->type == ESP_BT_GAP_DEV_PROP_COD) {
                    sink = prop_is_audio_sink(p);
                }
            }
            if (!sink) {
                break;   /* keep phones/laptops out of the portal's speaker list */
            }
            uint8_t buf[6 + 57];
            memcpy(buf, param->disc_res.bda, 6);
            size_t nl = strnlen(name, sizeof(buf) - 6);
            memcpy(buf + 6, name, nl);
            link_tx_send_control(PCM_LINK_CTRL_BT_SCAN_RESULT, buf,
                                 (uint16_t)(6 + nl));
            break;
        }
        if (s_peer_found) {
            break;
        }
        if (is_target(param, name, sizeof(name))) {
            ESP_LOGI(TAG, "found sink '%s', connecting", name[0] ? name : "?");
            memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
            s_peer_found = true;
            esp_bt_gap_cancel_discovery();
            send_bt_status(PCM_LINK_BT_CONNECTING);
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
            send_bt_status(PCM_LINK_BT_CONNECTED);
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        } else if (st == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            s_connected = false;
            s_peer_found = false;
            send_bt_status(PCM_LINK_BT_DISCONNECTED);
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

/* ----- AVRCP target: relay the car's transport buttons to the S3 -------- */
static void avrc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
    if (event != ESP_AVRC_TG_PASSTHROUGH_CMD_EVT) {
        return;
    }
    /* Act on the release edge so each press maps to one event. */
    if (param->psth_cmd.key_state != ESP_AVRC_PT_CMD_STATE_RELEASED) {
        return;
    }
    uint8_t cmd;
    switch (param->psth_cmd.key_code) {
    case ESP_AVRC_PT_CMD_PLAY:     cmd = PCM_LINK_AVRCP_PLAY;  break;
    case ESP_AVRC_PT_CMD_PAUSE:    cmd = PCM_LINK_AVRCP_PAUSE; break;
    case ESP_AVRC_PT_CMD_FORWARD:  cmd = PCM_LINK_AVRCP_NEXT;  break;
    case ESP_AVRC_PT_CMD_BACKWARD: cmd = PCM_LINK_AVRCP_PREV;  break;
    default: return;
    }
    link_tx_send_control(PCM_LINK_CTRL_AVRCP_EVENT, &cmd, 1);
}

static void avrcp_tg_init(void)
{
    esp_avrc_tg_init();
    esp_avrc_tg_register_callback(avrc_tg_cb);

    /* Allow the transport-control passthrough commands we relay. */
    esp_avrc_psth_bit_mask_t mask = { 0 };
    esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &mask, ESP_AVRC_PT_CMD_PLAY);
    esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &mask, ESP_AVRC_PT_CMD_PAUSE);
    esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &mask, ESP_AVRC_PT_CMD_FORWARD);
    esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &mask, ESP_AVRC_PT_CMD_BACKWARD);
    esp_avrc_tg_set_psth_cmd_filter(ESP_AVRC_PSTH_FILTER_ALLOWED_CMD, &mask);
}

void a2dp_bridge_start_scan(void)
{
    s_scanning = true;
    s_peer_found = false;
    ESP_LOGI(TAG, "orchestrated scan: reporting sinks to S3");
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}

void a2dp_bridge_pair(const uint8_t mac[6])
{
    s_scanning = false;
    esp_bt_gap_cancel_discovery();
    memcpy(s_peer_bda, mac, ESP_BD_ADDR_LEN);
    s_peer_found = true;
    ESP_LOGI(TAG, "pairing to %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    send_bt_status(PCM_LINK_BT_CONNECTING);
    esp_a2d_source_connect(s_peer_bda);
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

    /* AVRCP target so the car's steering-wheel transport buttons reach us. */
    avrcp_tg_init();

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
