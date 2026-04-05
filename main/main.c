#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "csi_handler.h"
#include "provision.h"

static const char *TAG = "main";

/* WiFi 连接事件组 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     10

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;

/* 从 NVS 读取 WiFi 配置 */
static esp_err_t nvs_read_wifi_config(char *ssid, size_t ssid_len,
                                       char *password, size_t pass_len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    err = nvs_get_str(handle, "ssid", ssid, &ssid_len);
    if (err != ESP_OK) { nvs_close(handle); return err; }

    /* 密码可以为空（开放网络），忽略 not-found 错误 */
    size_t p_len = pass_len;
    nvs_get_str(handle, "password", password, &p_len);

    nvs_close(handle);
    return ESP_OK;
}

/* 从 NVS 读取 WebSocket 服务端 URI */
static esp_err_t nvs_read_ws_uri(char *uri, size_t uri_len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("app_config", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, "ws_uri", uri, &uri_len);
    nvs_close(handle);
    return err;
}

/* WiFi 事件处理 */
static void wifi_event_handler(void *arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d",
                     s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi connect failed after %d retries", WIFI_MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void app_main(void) {
    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* 初始化网络栈（仅一次） */
    esp_netif_init();
    esp_event_loop_create_default();

    /* 读取 MAC 地址作为设备 ID */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[32];
    snprintf(device_id, sizeof(device_id),
             "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID (MAC): %s", device_id);
    csi_set_device_id(device_id);

    /* 检查是否已配置 WiFi */
    if (!provision_is_configured()) {
        ESP_LOGI(TAG, "No WiFi config found, starting provisioning AP mode...");
        provision_start_ap_mode();
        return;
    }

    /* 从 NVS 读取 WiFi 凭据 */
    char ssid[64]     = {0};
    char password[64] = {0};
    if (nvs_read_wifi_config(ssid, sizeof(ssid),
                              password, sizeof(password)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi config from NVS");
        provision_start_ap_mode();
        return;
    }
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    /* 初始化 WiFi Station */
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL,
                                         &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL,
                                         &instance_got_ip);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid,     ssid,     sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    /* 等待 WiFi 连接或失败 */
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "WiFi connection failed, entering provisioning mode");
        esp_wifi_stop();
        provision_start_ap_mode();
        return;
    }

    /* 读取 WebSocket 服务端 URI（默认回退值） */
    char ws_uri[128] = "ws://192.168.1.100:8765/csi";
    if (nvs_read_ws_uri(ws_uri, sizeof(ws_uri)) == ESP_OK) {
        ESP_LOGI(TAG, "WebSocket URI from NVS: %s", ws_uri);
    } else {
        ESP_LOGW(TAG, "ws_uri not set in NVS, using default: %s", ws_uri);
    }

    /* 初始化 CSI 和 WebSocket */
    csi_init();
    csi_ws_init(ws_uri);

    ESP_LOGI(TAG, "RuView firmware started. Device: %s", device_id);

    /* 主循环：时分复用 CSI 采集（10ms 采集 / 90ms 正常联网） */
    while (1) {
        esp_wifi_set_promiscuous(true);
        csi_start();
        vTaskDelay(pdMS_TO_TICKS(10));
        csi_stop();
        esp_wifi_set_promiscuous(false);
        vTaskDelay(pdMS_TO_TICKS(90));
    }
}
