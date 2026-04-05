#include "csi_handler.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "csi_handler";
static char s_device_id[32] = "robot-001";
static bool s_running = false;

void csi_set_device_id(const char *device_id) {
    strncpy(s_device_id, device_id, sizeof(s_device_id) - 1);
}

static void wifi_csi_callback(void *ctx, wifi_csi_info_t *info) {
    if (!s_running) return;
    // 将 CSI 数据序列化为 JSON 并通过 WebSocket 上报
    // TODO: 实现 WebSocket 发送
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "csi_data");
    cJSON_AddNumberToObject(root, "timestamp", (double)esp_timer_get_time() / 1000);
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    // 上报逻辑待实现
    cJSON_Delete(root);
}

void csi_init(void) {
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
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
