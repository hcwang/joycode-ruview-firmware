#pragma once
#include "esp_wifi.h"

/**
 * @brief 初始化 CSI 模块（注册回调，配置参数）
 */
void csi_init(void);

/**
 * @brief 启动 CSI 采集
 */
void csi_start(void);

/**
 * @brief 停止 CSI 采集
 */
void csi_stop(void);

/**
 * @brief 设置设备 ID（MAC 地址字符串）
 */
void csi_set_device_id(const char *device_id);

/**
 * @brief 初始化 WebSocket 客户端，连接到指定服务端 URI
 *        例如: "ws://192.168.1.100:8765/csi"
 */
void csi_ws_init(const char *server_uri);

/**
 * @brief 断开并销毁 WebSocket 客户端
 */
void csi_ws_deinit(void);
