# ESP-Claw AI Agent — 集成到 ESP-Handheld BSP

> 将 [ESP-Claw](https://github.com/espressif/esp-claw)（乐鑫 AI Agent 框架）作为 ota_1 固件，
> 与 ESP-Handheld（factory）组成双固件系统，通过菜单切换、START+B 返回。

---

## 目录

- [概述](#概述)
- [前置要求](#前置要求)
- [构建 ESP-Claw](#构建-esp-claw)
- [修改分区表](#修改分区表)
- [烧录双固件](#烧录双固件)
- [START+B 切回补丁](#startb-切回补丁)
- [文件说明](#文件说明)
- [附录：分区表参考](#附录分区表参考)

---

## 概述

双固件系统：ESP-Handheld（出厂）做游戏/工具，ESP-Claw（ota_1）做 AI 智能体。

| 固件 | 分区 | 说明 |
|------|------|------|
| ESP-Handheld | factory (0x10000) | 原系统：游戏、BLE HID、天气等 |
| ESP-Claw | ota_1 (0x810000) | 乐鑫 AI Agent：聊天/自动化/LLM |

切换方式：
- **ESP-Handheld → ESP-Claw**：菜单 → ESP-Claw 卡片 → 按 A
- **ESP-Claw → ESP-Handheld**：按 **START + B**（需打补丁）

---

## 前置要求

- ESP-IDF **v5.5.4**（与 ESP-Handheld 相同）
- `pip install esp-bmgr-assist`
- 16MB Flash + 8MB PSRAM 的 ESP32-S3 开发板

---

## 构建 ESP-Claw

### 1. 克隆 ESP-Claw

```bash
git clone https://github.com/espressif/esp-claw.git
cd esp-claw/application/edge_agent
```

### 2. 复制自定义板级定义

```bash
# 把本仓库的 boards/zistar/ 复制到 edge_agent/boards/ 下
cp -r <path-to>/esp-claw-integration/boards/zistar ./boards/
```

### 3. 生成配置并构建

```bash
# 安装 bmgr 工具
pip install esp-bmgr-assist

# 生成板级配置
idf.py bmgr -c ./boards -b zistar

# 构建
idf.py build
```

构建产物：
- `build/edge_agent.bin` — 主应用 (~3.2MB)
- `build/emote_assets.bin` — 表情/图标数据 (~2.4MB)
- `build/system.bin` — 系统配置 (~768KB)
- `build/storage.bin` — 用户数据 (~704KB)

---

## 修改分区表

ESP-Claw 需要额外的数据分区（emote / system / storage），
需要修改 ESP-Handheld 项目的 `partitions.csv`。

**原分区表：**
```
nvs       0x9000   16KB
otadata   0xD000   8KB
phy_init  0xF000   4KB
factory   0x10000  4MB     ← ESP-Handheld
ota_0     0x410000 4MB     ← Retro-Go
ota_1     0x810000 4MB     ← ESP-Claw
assets    0xC10000 ~4MB    ← 未使用的 SPIFFS（可移除）
```

**修改后（供参考）：**
```
nvs       0x9000   16KB
otadata   0xD000   8KB
phy_init  0xF000   4KB
factory   0x10000  4MB     ← ESP-Handheld
ota_0     0x410000 4MB     ← Retro-Go
ota_1     0x810000 4MB     ← ESP-Claw
emote     0xC10000 2.5MB   ← ESP-Claw 表情数据（SPIFFS）
system    0xE90000 768KB   ← ESP-Claw 系统配置（FAT）
storage   0xF50000 704KB   ← ESP-Claw 用户数据（FAT）
```

> assets 分区在代码中未被使用，可安全移除或用其空间。

**ESP-Handheld 的菜单修改（可选）：**
- `main/app/app_manager.c` 中 APP_ID_XIAOZHI 的逻辑切换 ota_1
- 如需更改菜单名称，修改 `main/ui/screens/menu_screen.c` 中 items 数组

---

## 烧录双固件

一次烧录所有分区（进入下载模式后执行）：

```bash
set B=<ESP-Handheld 构建目录>
set C=<ESP-Claw 构建目录>

python -m esptool --chip esp32s3 -p COM8 -b 460800 write_flash ^
    --flash_mode dio --flash_size 16MB --flash_freq 80m ^
    0x0       "%B%/bootloader/bootloader.bin" ^
    0x8000    "%B%/partition_table/partition-table.bin" ^
    0xD000    "%B%/ota_data_initial.bin" ^
    0x10000   "%B%/esp_handheld.bin" ^
    0x810000  "%C%/edge_agent.bin" ^
    0xC10000  "%C%/emote_assets.bin" ^
    0xE90000  "%C%/system.bin" ^
    0xF50000  "%C%/storage.bin"
```

也可使用 `tools/flash_dual_boot.bat`（需修改路径）。

---

## START+B 切回补丁

默认 ESP-Claw 没有返回 ESP-Handheld 的快捷键。
`patches/main.c.start-b.patch` 给 ESP-Claw 的 `main/main.c` 增加了：
- GPIO 初始化（START=GPIO6, B=GPIO5，内部上拉）
- 50ms 轮询 + 150ms 去抖
- 同时按下时：`esp_ota_set_boot_partition(factory)` + `esp_restart()`

**应用补丁：**
```bash
cd esp-claw/application/edge_agent
git apply <path-to>/esp-claw-integration/patches/main.c.start-b.patch
```

**手动操作：**
在 `main/main.c` 的 `app_main()` 中 `register_wifi_command();` 后添加：
```c
/* START+B → switch back to factory (ESP-Handheld) */
extern void start_b_monitor_start(void);
start_b_monitor_start();
```
并在文件末尾添加 `patches/main.c.start-b.patch` 中的函数。

---

## 文件说明

```
esp-claw-integration/
├── README.md                          ← 本文件
├── boards/zistar/                     ← ESP-Claw 自定义板级定义
│   ├── board_info.yaml                ← 板名、芯片、描述
│   ├── board_peripherals.yaml         ← 外设定义（I2C、I2S×2、SPI、LEDC）
│   ├── board_devices.yaml             ← 设备定义（音频、显示、背光）
│   ├── setup_device.c                 ← ST7789 面板偏移 + 初始化序列
│   └── sdkconfig.defaults.board       ← 板级 SDK 配置
├── tools/
│   ├── flash_esp_claw.bat             ← ESP-Claw 完整烧录
│   ├── flash_dual_boot.bat            ← 双固件烧录
│   └── switch_to_esp_handheld.bat     ← 从 ESP-Claw 切回 ESP-Handheld
├── patches/
│   └── main.c.start-b.patch           ← START+B 返回补丁
└── reference/
    └── partitions.csv                 ← 混合分区表参考
```

---

## 使用说明

完整使用文档见 `tools/ESP-Claw使用说明.md`。

快速入门：
1. ESP-Claw 启动后，屏幕显示小龙虾表情
2. 用手机连接热点 `esp-claw-xxxx`（无密码）
3. 浏览器打开 http://192.168.4.1
4. 配置 WiFi 和 LLM API Key
5. 保存重启，通过 Web/微信/Telegram 对话

---

## 附录：分区表参考

`reference/partitions.csv` 是经过验证的混合分区表，可供 ESP-Handheld 和 ESP-Claw 共用：

- NVS 及系统分区（0x9000～0xF000）两个固件共享
- factory / ota_0 / ota_1 各 4MB
- emote / system / storage 供 ESP-Claw 运行时使用
