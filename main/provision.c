#include "provision.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "provision";

bool provision_is_configured(void) {
    // 检查 NVS 中是否已保存 WiFi 配置
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    char ssid[64] = {0};
    size_t len = sizeof(ssid);
    err = nvs_get_str(handle, "ssid", ssid, &len);
    nvs_close(handle);
    return (err == ESP_OK && strlen(ssid) > 0);
}

void provision_start_ap_mode(void) {
    ESP_LOGI(TAG, "Starting AP mode for provisioning...");
    // 启动 AP 热点，提供配网 Web UI
    // TODO: 实现 HTTP 服务器接收 WiFi 配置
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "JoyRobot-Setup",
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "AP mode started. Connect to 'JoyRobot-Setup' to configure WiFi.");
}
