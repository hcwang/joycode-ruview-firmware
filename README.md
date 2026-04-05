# joycode-ruview-firmware

开心小金刚机器人 RuView WiFi CSI 感知固件（ESP32-S3）

## 功能
- WiFi CSI 采集（时分复用，ESP32-S3-WROOM-1）
- 人体存在检测、呼吸率/心率检测
- WebSocket 上报 CSI 数据至服务端
- AP 模式配网（首次上电）
- 设备注册（MAC 地址唯一标识）

## 依赖
- ESP-IDF v5.x
- ESP32-S3（不支持 ESP32-C3 或原版 ESP32）

## 构建
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## 相关仓库
- [服务端](https://github.com/hcwang/joycode-ruview-server)
- [iOS App](https://github.com/hcwang/joycode-ruview-ios)
- 参考：[RuView](https://github.com/ruvnet/RuView)
