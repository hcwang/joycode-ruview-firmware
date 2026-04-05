#include "csi_handler.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "csi_handler";
static char s_device_id[32] = "robot-001";
static bool s_running = false;
static esp_websocket_client_handle_t s_ws_client = NULL;

/* ------------------------------------------------------------------ */
/*  设备 ID                                                             */
/* ------------------------------------------------------------------ */

void csi_set_device_id(const char *device_id) {
    strncpy(s_device_id, device_id, sizeof(s_device_id) - 1);
    s_device_id[sizeof(s_device_id) - 1] = '\0';
}

/* ------------------------------------------------------------------ */
/*  WebSocket 事件处理                                                  */
/* ------------------------------------------------------------------ */

static void ws_event_handler(void *arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:
            /* 服务端下行消息，暂不处理 */
            ESP_LOGD(TAG, "WebSocket data received, len=%d", data->data_len);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/*  WebSocket 初始化 / 销毁                                             */
/* ------------------------------------------------------------------ */

void csi_ws_init(const char *server_uri) {
    if (s_ws_client != NULL) {
        ESP_LOGW(TAG, "WebSocket already initialized, skipping");
        return;
    }
    esp_websocket_client_config_t ws_cfg = {
        .uri = server_uri,
        .reconnect_timeout_ms = 3000,
        .network_timeout_ms   = 5000,
    };
    s_ws_client = esp_websocket_client_init(&ws_cfg);
    if (s_ws_client == NULL) {
        ESP_LOGE(TAG, "Failed to init WebSocket client");
        return;
    }
    esp_websocket_register_events(s_ws_client,
                                  WEBSOCKET_EVENT_ANY,
                                  ws_event_handler,
                                  NULL);
    esp_websocket_client_start(s_ws_client);
    ESP_LOGI(TAG, "WebSocket client started, uri=%s", server_uri);
}

void csi_ws_deinit(void) {
    if (s_ws_client == NULL) return;
    esp_websocket_client_stop(s_ws_client);
    esp_websocket_client_destroy(s_ws_client);
    s_ws_client = NULL;
    ESP_LOGI(TAG, "WebSocket client destroyed");
}

/* ------------------------------------------------------------------ */
/*  CSI 回调：序列化并通过 WebSocket 上报                               */
/* ------------------------------------------------------------------ */

static void wifi_csi_callback(void *ctx, wifi_csi_info_t *info) {
    if (!s_running) return;
    if (info == NULL) return;

    /* 构建 JSON 消息 */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return;

    cJSON_AddStringToObject(root, "type", "csi_data");
    /* timestamp 单位：毫秒 */
    cJSON_AddNumberToObject(root, "timestamp",
                            (double)esp_timer_get_time() / 1000.0);
    cJSON_AddStringToObject(root, "device_id", s_device_id);

    /* 原始 CSI 数据（int8 数组）编码为 JSON 数字数组 */
    if (info->buf != NULL && info->len > 0) {
        cJSON *csi_arr = cJSON_CreateArray();
        if (csi_arr != NULL) {
            for (int i = 0; i < info->len; i++) {
                cJSON_AddItemToArray(csi_arr,
                                     cJSON_CreateNumber((double)info->buf[i]));
            }
            cJSON_AddItemToObject(root, "csi_raw", csi_arr);
        }
    }

    /* RSSI / 信道信息 */
    cJSON_AddNumberToObject(root, "rssi",    (double)info->rx_ctrl.rssi);
    cJSON_AddNumberToObject(root, "channel", (double)info->rx_ctrl.channel);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) return;

    /* 通过 WebSocket 发送（非阻塞，若未连接则静默丢弃） */
    if (s_ws_client != NULL &&
        esp_websocket_client_is_connected(s_ws_client)) {
        int ret = esp_websocket_client_send_text(
            s_ws_client, json_str, strlen(json_str), pdMS_TO_TICKS(200));
        if (ret < 0) {
            ESP_LOGW(TAG, "WebSocket send failed");
        }
    } else {
        ESP_LOGD(TAG, "WebSocket not connected, dropping CSI frame");
    }

    free(json_str);
}

/* ------------------------------------------------------------------ */
/*  CSI 模块初始化 / 启停                                               */
/* ------------------------------------------------------------------ */

void csi_init(void) {
    wifi_csi_config_t csi_config = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = true,
        .manu_scale        = false,
    };
    esp_wifi_set_csi_config(&csi_config);
    esp_wifi_set_csi_rx_cb(wifi_csi_callback, NULL);
    ESP_LOGI(TAG, "CSI initialized");
}

void csi_start(void) {
    s_running = true;
    esp_wifi_set_csi(true);
    ESP_LOGI(TAG, "CSI started");
}

void csi_stop(void) {
    s_running = false;
    esp_wifi_set_csi(false);
    ESP_LOGI(TAG, "CSI stopped");
}
