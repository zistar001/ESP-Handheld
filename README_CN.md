# ESP-Handheld

[![Build](https://github.com/zistar001/ESP-Handheld/actions/workflows/build.yml/badge.svg)](https://github.com/zistar001/ESP-Handheld/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

基于 **ESP32-S3** 的开源掌机固件。集成了复古游戏模拟器、LVGL 图形界面、天气显示、BLE 蓝牙键盘鼠标、WiFi 音频串流、占卜工具等功能。

## 功能特点

- 🎮 **游戏模拟器** — 内置 NES (nofrendo) 模拟器 + Retro-Go 独立系统（支持 FC/GB/GBC/SMS/PCE/SFC）
- 🖥️ **LVGL 图形界面** — 240×280 ST7789 屏幕，状态栏、主屏、应用菜单、设置
- 🌤️ **天气面板** — 和风天气 API（7天预报 + 实时天气）
- ⌨️ **BLE 蓝牙 HID** — 蓝牙键盘鼠标（Bluedroid 协议栈）
- 🎙️ **WiFi 音频** — 按住说话，通过 UDP 推送到 PC
- 🎵 **频谱分析** — FFT 可视化，支持参数调节
- 🎲 **占卜工具** — 易经六十四卦 + 小六壬
- 🎨 **主题系统** — 6 种内置配色，支持 5 色槽混搭
- 🔄 **双固件切换** — 一键切换 XiaoZhi AI 或 Retro-Go 系统
- 💤 **电源管理** — Light Sleep 待机（5-10mA）
- 🎚️ **音频** — MAX98357 数字功放 + MSM261 数字麦克风

## 硬件要求

| 组件 | 规格 |
|-----------|------|
| 主控 | ESP32-S3（需要 8MB PSRAM） |
| Flash | 16MB |
| PSRAM | 8MB Octal |
| 屏幕 | ST7789 240×280 SPI |
| 功放 | MAX98357 (I2S) |
| 麦克风 | MSM261 (I2S 数字麦克风) |
| IMU | LSM6DS3TR-C（可选） |
| 温湿度 | AHT20（可选） |
| SD 卡 | SPI 模式（可选） |
| 按键 | 7 个物理按键（上/下/左/右/A/B/START） |

> **引脚定义和硬件适配见 [docs/HARDWARE.md](docs/HARDWARE.md)**

## 快速开始

### 前置条件

1. 安装 **ESP-IDF v5.5.4**（目标芯片选择 `esp32s3`）：
   - [Windows 安装指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html)
   - [Linux/macOS 安装指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/linux-macos-setup.html)
2. 安装 **Git**

### Windows

```powershell
# 在 ESP-IDF PowerShell 环境中运行：
git clone --recursive https://github.com/zistar001/ESP-Handheld.git
cd ESP-Handheld
.\scripts\setup.ps1
```

配置天气 API Key：
```powershell
.\scripts\setup.ps1 -Menuconfig
# 进入：ESP-Handheld Configuration → Weather API Key
```

### Linux / macOS

```bash
git clone --recursive https://github.com/zistar001/ESP-Handheld.git
cd ESP-Handheld
. $IDF_PATH/export.sh
./scripts/setup.sh
```

### 烧录

```bash
# 全新烧录（含 bootloader + 分区表 + OTA 数据）：
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin

# 仅更新应用：
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x10000 build/esp_handheld.bin
```

> `(PORT)` 替换为实际串口：Windows 上为 `COM3`，Linux 上为 `/dev/ttyACM0`。

## 需要注册的服务

| 服务 | 是否必须 | 用途 | 注册地址 |
|---------|-----------|------|---------|
| [和风天气](https://devapi.qweather.com) | 可选 | 天气显示（不配则禁用） | 免费注册获取 Key |

**配置方法：** `idf.py menuconfig` → "ESP-Handheld Configuration" → "Weather API Key"

> 详见 [docs/API_KEYS.md](docs/API_KEYS.md)

## 分区布局

| 分区 | 偏移 | 大小 | 内容 |
|-----------|--------|------|---------|
| factory | 0x10000 | 4MB | ESP-Handheld（本固件） |
| ota_0 | 0x410000 | 4MB | Retro-Go 独立游戏系统 |
| ota_1 | 0x810000 | 4MB | XiaoZhi AI 固件 |
| assets | 0xC10000 | ~4MB | SPIFFS 资源 |

## 项目结构

```
main/
  main.c              — 入口函数、初始化序列、按键分发
  bsp/                — 板级支持（st7789、sd_card、key_driver）
  app/                — 应用框架（app_manager、launcher、menu）
  modules/            — 功能模块（audio、imu、weather、ble 等）
  ui/                 — LVGL 界面、组件、显示驱动
components/
  lvgl/               — LVGL v8.4（子模块）
  retro-core/         — NES 模拟器核心（nofrendo）
  retro-go/           — Retro-Go 运行时
  esp-wifi-connect/   — WiFi 配置库（子模块）
```

## 文档

- [操作手册](docs/USER_MANUAL_CN.md) — 完整使用指南（按键说明、应用操作、WiFi/蓝牙等）
- [编译指南](docs/BUILD_CN.md) — 各平台详细编译步骤
- [烧录指南](docs/FLASH_CN.md) — 所有烧录方式
- [硬件适配](docs/HARDWARE_CN.md) — 引脚定义和 PCB 指南
- [API 密钥](docs/API_KEYS_CN.md) — 服务注册和配置

## 开源协议

本项目使用 **GNU General Public License v2.0** — 详见 [LICENSE](LICENSE)。

**为什么用 GPL v2？** 本固件（factory 分区）编译并链接了 nofrendo NES 模拟器核心，而 nofrendo 采用 GPL v2 许可证。根据 GPL 的要求，合并后的作品必须使用 GPL v2 发布。

包含以下开源组件：
- [nofrendo](https://github.com/ducalex/retro-go/tree/master/retro-core/components/nofrendo) — **GPL v2**（编译链接在主固件中）
- [Retro-Go](https://github.com/ducalex/retro-go) — GPL v2（独立 ota_0 固件，单独二进制）
- [LVGL](https://github.com/lvgl/lvgl) — MIT
- [esp-wifi-connect](https://github.com/78/esp-wifi-connect) — 未明确声明许可证（子模块引用）
- [XiaoZhi AI](https://github.com/78/xiaozhi-esp32) — MIT（独立 ota_1 固件，单独二进制）
