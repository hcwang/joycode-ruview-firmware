#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "csi_handler.h"
#include "provision.h"

static const char *TAG = "main";

void app_main(void) {
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 读取 MAC 地址作为设备 ID
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID (MAC): %s", device_id);
    csi_set_device_id(device_id);

    // 检查是否已配置 WiFi
    if (!provision_is_configured()) {
        ESP_LOGI(TAG, "No WiFi config found, starting provisioning AP mode...");
        provision_start_ap_mode();
        return;
    }

    // 初始化 WiFi Station 模式
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    // TODO: 从 NVS 读取 SSID/Password 并连接
    esp_wifi_start();

    // 初始化 CSI（时分复用：每 100ms 切换一次采集）
    csi_init();

    ESP_LOGI(TAG, "RuView firmware started. Device: %s", device_id);

    while (1) {
        // 时分复用：切换到 promiscuous 模式采集 CSI
        esp_wifi_set_promiscuous(true);
        csi_start();
        vTaskDelay(pdMS_TO_TICKS(10)); // 采集 10ms
        csi_stop();
        esp_wifi_set_promiscuous(false);
        vTaskDelay(pdMS_TO_TICKS(90)); // 正常联网 90ms
    }
}
