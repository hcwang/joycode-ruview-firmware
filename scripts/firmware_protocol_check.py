"""
固件上报协议验证脚本（铁芯编写）
模拟 ESP32-S3 的 WebSocket 上报行为，验证服务端能正确处理固件数据格式

运行：python scripts/firmware_protocol_check.py
"""
import asyncio
import json
import sys
import websockets
import urllib.request
import time

SERVER_URL = "http://localhost:8080"
WS_URL = "ws://localhost:8080"

# 固件上报的 CSI 帧格式（来自 csi_handler.c）
FIRMWARE_FRAME = {
    "type": "csi_data",
    "timestamp": int(time.time() * 1000),
    "device_id": "aabbccddeeff",   # MAC 地址（无冒号，来自 esp_wifi_get_mac）
    "csi_raw": [
        10, -5, 8, -3, 12, -7, 9, -4,
        6, -2, 11, -8, 7, -6, 13, -1,
        5, -9, 14, -3, 8, -7, 10, -5,
        9, -4, 11, -6, 7, -2, 12, -8,
        6, -5, 13, -3, 9, -7, 11, -4,
        8, -6, 10, -2, 7, -8, 12, -5,
        11, -3, 9, -7, 13, -4, 8, -6
    ]  # 52 个子载波 IQ 值（ESP32-S3 HT20 标准）
}

async def test_firmware_uplink():
    print("=== 固件上报协议验证开始 ===")

    # 1. 模拟设备注册（固件首次连接时会注册）
    print("[1] 模拟固件设备注册...")
    mac = FIRMWARE_FRAME["device_id"]
    req = urllib.request.Request(
        f"{SERVER_URL}/api/devices/enroll",
        data=json.dumps({
            "mac": ":".join(mac[i:i+2] for i in range(0, 12, 2)),
            "name": "esp32s3-test-node",
            "user_id": "firmware-test"
        }).encode(),
        headers={"Content-Type": "application/json"},
        method="POST"
    )
    with urllib.request.urlopen(req) as resp:
        enroll = json.loads(resp.read())
    device_id = enroll["device_id"]
    print(f"  ✅ 注册成功: device_id={device_id}, token={enroll['token'][:8]}...")

    # 2. 模拟固件持续上报 CSI 帧（时分复用，约 50Hz）
    print("[2] 模拟固件 CSI 上报（10帧）...")
    async with websockets.connect(f"{WS_URL}/ws/csi/{device_id}") as ws:
        for i in range(10):
            frame = dict(FIRMWARE_FRAME)
            frame["timestamp"] = int(time.time() * 1000) + i * 20  # 20ms 间隔
            await ws.send(json.dumps(frame))
            resp = json.loads(await asyncio.wait_for(ws.recv(), timeout=3.0))

            # 验证服务端响应格式（固件不直接用，但 CI 用来验证协议）
            assert resp["type"] == "csi_data", f"帧{i}: type 字段错误"
            assert resp["device_id"] == device_id, f"帧{i}: device_id 不匹配"
            assert "presence" in resp, f"帧{i}: 缺少 presence"
            assert "vitals" in resp, f"帧{i}: 缺少 vitals"

        print(f"  ✅ 10帧上报全部成功，服务端响应格式正确")
        print(f"  最后一帧 vitals: {resp.get('vitals')}")

    print("=== 固件上报协议验证完成 ===")

if __name__ == "__main__":
    asyncio.run(test_firmware_uplink())
