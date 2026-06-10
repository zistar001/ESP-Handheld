# ESP32-S3 智能小手柄 — 项目参考手册

> **最后更新:** 2026-06-05
> **硬件状态:** 已到货，基本功能测试通过
> **构建状态:** Game固件编译通过并运行正常 | 小智AI编译通过并已烧录
> **IDF 版本:** v5.5.4

---

## 目录

1. [项目概述](#1-项目概述)
2. [硬件规格与引脚定义](#2-硬件规格与引脚定义)
3. [双系统架构](#3-双系统架构)
4. [软件架构总览](#4-软件架构总览)
5. [模块详解](#5-模块详解)
6. [构建与烧录](#6-构建与烧录)
7. [开发路线图](#7-开发路线图)
8. [资源链接](#8-资源链接)
9. [设计决策记录](#9-设计决策记录)
10. [硬件调试指南](#10-硬件调试指南)

---

## 1. 项目概述

基于 ESP32-S3 的智能手持设备，具备以下功能：

- **游戏:** NES 模拟器（~44 种 MMC mapper，支持 SD 卡 ROM 浏览器）
- **PC 远控:** BLE HID 键鼠 + 空中飞鼠（IMU 陀螺仪）
- **信息显示:** 天气（和风 API）、温湿度（AHT20）、IMU 姿态
- **AI 交互:** 双系统切换至 XiaoZhi AI（独立固件，ota_1 分区）
- **音频:** ES8311 音频输出 + ES7210 双麦输入（I2S 共享总线）
- **电源管理:** DFS 动态调频 + Light Sleep 休眠（按键唤醒）

---

## 2. 硬件规格与引脚定义

### 主控

| 项目 | 规格 |
|------|------|
| SoC | ESP32-S3（双核 Xtensa LX7 @ 240MHz） |
| Flash | 16MB (Quad SPI) |
| PSRAM | 8MB (Quad, Octal 可选) |
| LCD | ST7789, 240×280, SPI (60MHz DMA) |
| 按键 | 7 个（上/下/左/右/A/B/Start） |
| 传感器 | AHT20 (温湿度) + LSM6DS3TR-C (6 轴 IMU) |
| 音频 | ES8311 (DAC 输出) + ES7210 (ADC 双麦输入) |
| 电池 | 单节锂电池, ADC 分压检测 (GPIO3) |
| SD 卡 | SPI 模式, FATFS |

### 完整引脚表 (2026-06-05 更新)

```
1.69' LCD屏幕 (SPI2_HOST)
  LCD_BL    → IO9     (背光PWM)
  LCD_DC    → IO10    (数据/命令)
  LCD_CS    → IO11    (片选)
  LCD_SCK   → IO12    (时钟)
  LCD_MOSI  → IO13    (数据)
  LCD_RES   → IO14    (复位)

TF卡 (SPI, 与LCD共用SPI2_HOST)
  SD_CMD    → IO13    (MOSI, 与LCD共用)
  SD_CLK    → IO12    (时钟, 与LCD共用)
  SD_DATA   → IO8     (MISO)
  SD_CS     → IO18
传感器 I2C (I2C_NUM_0, 100KHz)
  IMU_SDA   → IO2
  IMU_SCL   → IO1
  TEMP_SDA  → IO2     (与IMU共用)
  TEMP_SCL  → IO1     (与IMU共用)

音频 I2S (模组内部, ES8311+ES7210 共享)
  MCLK      → IO40    (256× fs = 4.096MHz @16kHz)
  BCLK      → IO39
  WS        → IO45
  DOUT      → IO48    (ESP32 → ES8311, TX)
  DIN       → IO21    (ES7210 → ESP32, RX)
  PA_EN     → IO46    (功放使能)

音频 I2C (I2C_NUM_1, 模组内部Codec控制)
  SDA       → IO38
  SCL       → IO47

MIC 麦克风 (ES7210 模拟输入, 非 ESP32 GPIO)
  MIC1_IN   → ES7210 CH1+
  MIC1_IP   → ES7210 CH1-
  MIC2_IN   → ES7210 CH2+
  MIC2_IP   → ES7210 CH2-

喇叭/音频输出
  VOP       → IO3     (功放输出+)
  VON       → IO46    (功放输出-)

按键 (低电平有效, 内置上拉)
  上        → IO5
  下        → IO6
  左        → IO4
  右        → IO7
  A         → IO17    (确认)
  B         → IO16    (返回)
  START     → IO15    (开始/菜单)
  BOOT      → BOOT    (下载模式)
  RESET     → RESET   (复位)

LED灯
  LED       → IO18

电池/充电
  BAT_ADC   → IO3     (ADC1_CHANNEL_2, 12bit, 1:2 分压)
  CHRG      → IO41    (低电平=充电中)
  DONE      → IO42    (高电平=已充满)
```

### I2C 设备地址

| 设备 | 7-bit 地址 | 所在总线 |
|------|-----------|---------|
| AHT20 | 0x38 | 传感器 I2C (GPIO 1/2) |
| LSM6DS3TR-C | 0x6A | 传感器 I2C (GPIO 1/2) |
| ES8311 | 0x18 | 音频 I2C (GPIO 38/47) |
| ES7210 | 0x40 | 音频 I2C (GPIO 38/47) |

---

## 3. 双系统架构

### 分区表

```
16MB Flash Layout:
  bootloader  : 0x000000 (~32KB)
  nvs         : 0x009000 (16KB)   — WiFi/RTC/校准
  otadata     : 0x00D000 (8KB)    — OTA 启动选择
  phy_init    : 0x00F000 (4KB)    — RF 校准
  factory     : 0x010000 (4MB)    — ✅ Game 固件 (ESP_BSP)
  ota_0       : 0x410000 (4MB)    — Game OTA 升级槽 (预留)
  ota_1       : 0x810000 (4MB)    — ✅ XiaoZhi AI 固件
  assets      : 0xC10000 (~4MB)   — SPIFFS 资源
```

### 切换逻辑

```
菜单选择 AI →
  ├─ 检测当前运行分区 (esp_ota_get_running_partition)
  ├─ factory/ota_0 → 目标 = ota_1 (XiaoZhi)
  ├─ ota_1         → 目标 = factory (Game)
  ├─ esp_ota_set_boot_partition(target)
  └─ esp_restart()
```

- 两系统共享同一份 bootloader + 分区表
- XiaoZhi AI 需用同一份 `partitions.csv` 编译，使其 app 定位在 0x810000
- XiaoZhi 侧禁用 OTA 功能（避免写错 OTA 槽），更新走 USB 直刷 ota_1

---

## 4. 软件架构总览

```
main/
├── main.c                    # 入口: 初始化所有模块, 创建任务
├── CMakeLists.txt            # 组件构建配置, REQUIRES 依赖
│
├── bsp/                      # 板级支持包
│   ├── bsp_board.c/h         # 板级初始化 (GPIO/SPI/I2C/PWM)
│   ├── st7789_driver.c/h     # ST7789 LCD 驱动 (SPI DMA, 60MHz)
│   ├── sd_card.c/h           # SD 卡 FATFS 挂载
│   └── key_driver.c/h        # 按键消抖扫描 (3次采样, 10ms)
│
├── app/                      # 应用管理
│   ├── app_manager.c/h       # 三态状态机: Launcher→Menu→Running
│   ├── launcher.c/h          # 主界面 (时间/天气/IMU/快捷入口)
│   ├── menu.c/h              # 3列网格菜单
│   └── rom_browser.c/h       # SD卡 *.nes 列表选择
│
├── modules/                  # 功能模块
│   ├── nes/                  # NES 模拟器
│   │   └── nes_wrapper.c/h   # → nes_core 组件
│   │
│   ├── sensor/               # 温湿度传感器
│   │   └── aht20_driver.c/h  # AHT20 I2C 驱动
│   │
│   ├── imu/                  # 六轴惯性测量
│   │   └── imu_driver.c/h    # LSM6DS3TR-C 驱动 + 姿态解算
│   │
│   ├── pc_remote/            # PC 远控
│   │   ├── ble_hid.c/h       # BLE HID 键盘+鼠标 (Bluedroid)
│   │   ├── air_mouse.c/h     # 空中飞鼠 (IMU→鼠标)
│   │   └── wifi_audio.c/h    # WiFi 音频 (预留)
│   │
│   ├── audio/                # 音频子系统
│   │   ├── es8311_driver.c/h # ES8311 DAC 薄封装
│   │   ├── es7210_driver.c/h # ES7210 ADC 薄封装
│   │   └── box_audio_codec.c/h # 完整音频子系统 (I2S TX+RX)
│   │
│   ├── wifi_manager/         # WiFi 管理
│   │   ├── wifi_manager.c/h  # C API 层
│   │   ├── wifi_bridge.cpp/h # C++ 桥接 → esp-wifi-connect 组件
│   │   └── wifi_provisioning.c/h # 转发桩 (向后兼容)
│   │
│   ├── weather/              # 天气
│   │   └── weather.c/h       # 和风 API HTTPS 查询
│   │
│   ├── time_sync/            # 时间同步
│   │   └── time_sync.c/h     # SNTP + NVS 备份 + UTC+8
│   │
│   ├── settings/             # 设置管理
│   │   └── settings_manager.c/h # NVS 9字段结构体
│   │
│   ├── power/                # 电源
│   │   └── battery_monitor.c/h # ADC 电池检测 + 充电状态
│   │
│   └── xiaozhi/              # 小智 AI (占位)
│       └── xiaozhi_wrapper.c/h # 分区切换, 不自包含固件
│
├── ui/                       # LVGL 图形界面
│   ├── display_driver.c/h    # LVGL display driver + 缓冲区
│   ├── screens/
│   │   ├── main_screen.c/h   # 主界面 (时间/温度/IMU)
│   │   ├── menu_screen.c/h   # 菜单网格
│   │   └── settings_screen.c/h # 设置页面 (9字段, 滚动)
│   └── components/
│       └── status_bar.c/h    # 状态栏 (WiFi/BLE/音量/亮度/电量)
│
└── components/               # 外部组件
    ├── esp-wifi-connect/     # WiFi 管理器 (github.com/78/esp-wifi-connect)
    ├── espressif__esp_codec_dev/ # 音频编解码设备驱动
    ├── nes_core/             # NES 模拟器核心
    └── lvgl/                 # LVGL v8.4 图形库
```

### 任务分配 (FreeRTOS)

| 任务 | 栈大小 | 优先级 | 核心 | 功能 |
|------|--------|--------|------|------|
| key | 2048 | 5 | 0 | 按键扫描 |
| sensor | 3072 | 3 | 0 | AHT20 + 主界面更新 |
| imu | 3072 | 3 | 0 | IMU 读取 + 姿态 |
| airmouse | 3072 | 3 | 0 | 空中飞鼠计算 |
| batt | 3072 | 2 | 0 | 电池监控 (10s) |
| pm | 2048 | 1 | 0 | 电源管理 (5s 检查休眠) |

### 三态状态机

```
Launcher (主界面) ──[Start键]──→ Menu (应用菜单)
Menu ──[A键选择]──→ Running (应用运行)
Running ──[B/Start]──→ Menu
Menu ──[B/Start]──→ Launcher
```

---

## 5. 模块详解

### 5.1 显示 (ST7789)

- **驱动:** `spi_device_transmit` DMA 模式, 60MHz
- **分辨率:** 240×280, RGB565 (16bit)
- **缓冲区:** PSRAM, 40 行缓冲 (240×40×2 = 19.2KB)
- **LVGL:** v8.4, 30FPS 刷新, NES 运行时会暂停 LVGL 刷新
- **休眠:** sleep_in/sleep_out 命令, 配合 Light Sleep

### 5.2 按键

- 7 个按键, 低电平有效, 内置上拉
- 软件消抖: 3 次采样, 10ms 间隔
- 回调模式: `key_driver_init(key_cb_t)` 注册回调
- NES 模式下: 支持长按（press 和 release 都转发给模拟器）
- NES 退出: B + START 组合键发送 quit 事件

### 5.3 NES 模拟器

- **来源:** [esp32s3_nes_gamer](https://github.com/planevina/esp32s3_nes_gamer)
- **组件:** `nes_core` (内置 6502 CPU + PPU)
- **Mapper:** 45 种 MMC mapper 支持
- **ROM:** SD 卡 `/sdcard/roms/*.nes`, PSRAM 缓存 (1MB)
- **显存:** PSRAM 分配 60KB (256×240 像素)
- **ROM 浏览器:** `rom_browser_enter()` 扫描并列表显示
- **音频:** 暂时禁用 (`SOUND_ENABLED=0`)
- **按键映射:** UP/DOWN/LEFT/RIGHT/A/B/START 直通
- **退出:** B + START 组合键

### 5.4 BLE HID

- **协议:** Bluedroid BLE, HID 键盘 + 鼠标
- **特性:** 安全配对、广播、断线重连
- **Passkey:** 硬编码 "1234"（待改为随机）
- **API:**
  - `ble_hid_send_key(modifier, key)` — 键盘按键
  - `ble_hid_send_mouse(buttons, dx, dy)` — 鼠标移动

### 5.5 空中飞鼠

- **原理:** IMU 陀螺仪数据 → 低通滤波 → 零偏校准 → 死区 → 鼠标 delta
- **灵敏度:** 由设置 `air_mouse_sens` 控制 (0.1~5.0)
- **启动条件:** BLE HID 已连接 + 在空中飞鼠 App 中

### 5.6 WiFi 管理

- **组件:** `78/esp-wifi-connect` v3.1.4 (MIT)
- **架构:** WifiManager 单例 + SsidManager 多 SSID 存储
- **配网:** Soft-AP + DNS 劫持 + 内置 Web 配置页
- **迁移:** 旧版 `settings_manager` WiFi 数据自动迁移到 SsidManager
- **首次启动:** 检测无已保存 SSID → 自动进入配网模式
- **C++ 桥接:** `wifi_bridge.cpp` 封装 C++ 单例为 C 函数

### 5.7 天气

- **API:** 和风天气 `devapi.qweather.com/v7/weather/now`
- **位置:** 泉州 (LocationID: 101230501)
- **更新间隔:** 后台每 30 分钟自动更新
- **依赖:** WiFi STA + HTTPS + cJSON

### 5.8 音频子系统

```
I2S_NUM_0 (共享总线)
  ├─ TX (标准模式) → ES8311 DAC → 扬声器 (PA GPIO46)
  └─ RX (TDM 4槽)  → ES7210 ADC → 双麦克风
```

- **采样率:** 16kHz, 16bit
- **I2C 地址:** ES8311=0x18, ES7210=0x40
- **音量控制:** `box_audio_set_volume(0-100)`, 与设置页面联动
- **麦克风增益:** `box_audio_set_mic_gain(0-100)`
- **待硬件验证:** I2C 探测、I2S 时序、TDM 双麦读取、音质、AEC

### 5.9 电池/电源

- **ADC:** 12bit, 3.3V 参考, 1:2 分压 → 实际电压 = ADC×3
- **百分比:** 3000mV~4200mV 线性映射 0~100%
- **充电状态:** CHRG=0 + DONE=1 = 充电中, DONE=1 = 满电
- **读取周期:** 每 10 秒, 10 次平均去抖
- **DFS:** 240MHz↔40MHz 动态调频 (ESP_PM_CPU_FREQ_MAX lock)
- **休眠:** Light Sleep, 按键唤醒, 超时可配 (默认 300s)
- **功耗优化:** 休眠前释放 max-freq lock + ST7789 sleep-in + I2S 停用

### 5.10 时间同步

- **SNTP 服务器:** ntp.aliyun.com + pool.ntp.org
- **时区:** UTC+8 (CST-8, 中国标准时间)
- **NVS 备份:** 最后同步时间持久化, 重启时恢复
- **局限性:** 首次启动无 WiFi 时显示 `--:--`; NVS 备份时间可能不精确

### 5.11 设置管理

- **存储:** NVS, 命名空间 "settings"
- **字段:**

| 字段 | 类型 | 范围 | 默认 |
|------|------|------|------|
| volume | uint8 | 0-100 | 70 |
| brightness | uint8 | 0-100 | 100 |
| mic_gain | uint8 | 0-100 | 50 |
| air_mouse_sens | float | 0.1-5.0 | 1.0 |
| wifi_ssid | char[32] | - | "" |
| wifi_pass | char[64] | - | "" |
| sleep_enabled | bool | - | true |
| sleep_timeout_sec | uint16 | 10-3600 | 300 |
| key_map | uint8[7] | - | 默认映射 |

### 5.12 状态栏

- **显示内容:** WiFi 图标、BLE 图标、音量、亮度、电量 (百分比+电池图标)
- **更新方式:** LVGL 定时器, app_manager 状态变化时主动刷新

---

## 6. 构建与烧录

详见 `docs/flash-guide.md`。简要步骤:

```bash
# 构建 Game 固件
cd D:\code\claudecode\ESP_BSP
idf.py build

# 烧录 Game 固件 (完整)
idf.py -p COM7 erase-flash
idf.py -p COM7 flash
```

### 小智 AI 构建与烧录

小智AI源码位于 `xiaozhi-esp32-main/`，使用 gezipai 板级配置。

**IO 映射（已适配本硬件）:**

| 功能 | GPIO | 说明 |
|------|------|------|
| BOOT 按键（按住说话） | IO17 | 按键A |
| 音量+ | IO5 | UP键（与游戏机共用） |
| 音量- | IO6 | DOWN键（与游戏机共用） |
| LED 指示灯 | IO18 | 独立LED |
| 音频 I2S | IO39/40/45/21/48 | ES8311+ES7210（与Game共用） |
| 音频 I2C | IO38/47 | Codec控制总线 |
| LCD SPI | IO9~14 | ST7789（与Game共用） |
| 电池 ADC | IO3 | 充电状态 IO41/42 |

**构建步骤:**

```bash
cd D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main
# 需要启用组件管理器下载依赖 (IDF_COMPONENT_MANAGER=1)
# 使用 _build.bat 构建（自动配置环境）
_build.bat
```

**烧录（独立运行，烧到 factory 分区）:**

```bash
# 擦除并完整烧录
esptool.py -p COM7 erase_flash
esptool.py -p COM7 -b 460800 write_flash 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/xiaozhi.bin
```

**烧录（双系统，仅更新小智到 ota_1）:**

```bash
esptool.py -p COM7 -b 921600 write_flash 0x810000 build/xiaozhi.bin
```

> **注意:** 首次烧录需要 bootloader + 分区表 + ota_data。使用 `_flash.bat` 可一键完成。

**构建环境:** ESP-IDF v5.5.4, Python 3.11, 需要:
- cmake 3.30+
- ninja 1.12+
- xtensa-esp-elf 工具链 14.2.0+
- 组件管理器需启用 (`IDF_COMPONENT_MANAGER=1`)，首次构建需联网下载托管组件

**组件管理器:** ESP_BSP已禁用 (`IDF_COMPONENT_MANAGER=0`), 所有组件本地托管。小智AI必须启用 (`IDF_COMPONENT_MANAGER=1`), 首次需联网下载。

---

## 7. 开发路线图

### 已完成 (软件编码)

- [x] BSP 板级初始化 (SPI/I2C/PWM/GPIO)
- [x] ST7789 LCD 驱动 (60MHz DMA)
- [x] SD 卡 FATFS 挂载
- [x] 按键驱动 (7键, 软件消抖)
- [x] LVGL v8.4 完整 UI 系统
- [x] 主界面 (时间/天气/温度/室内)
- [x] 状态栏 (WiFi/BLE/音量/亮度/电量)
- [x] 应用三态状态机
- [x] 3 列网格菜单 (6 App)
- [x] 设置页面 (9 字段, 滚动)
- [x] 关于页面 (显示当前系统/分区/版本)
- [x] NES 模拟器 (~44 mapper)
- [x] ROM 浏览器 (SD 卡 *.nes)
- [x] AHT20 温湿度读取
- [x] LSM6DS3 六轴 IMU + 姿态解算 (地址0x6B, WHO_AM_I=0x6A)
- [x] BLE HID 键盘 + 鼠标 (Bluedroid, Just Works配对)
- [x] 空中飞鼠 (IMU 陀螺仪→BLE鼠标, 零偏校准+滤波+死区)
- [x] ⌨ Kbd App (键盘HID模式, A=Enter, B=ESC)
- [x] 🖱 Mouse App (飞鼠+鼠标键, 三级灵敏度)
- [x] WiFi 管理 (esp-wifi-connect, 多 SSID, Soft-AP 配网)
- [x] 天气 (和风 API, 30 分钟自动更新, gunzip修复)
- [x] 时间同步 (SNTP + NVS 备份)
- [x] 电池 ADC 监控 (电压/百分比/充电状态)
- [x] DFS 动态调频 + Light Sleep
- [x] ES8311 音频输出驱动
- [x] ES7210 双麦输入驱动
- [x] BoxAudioCodec 音频子系统
- [x] 双系统切换 (factory ↔ ota_1)
- [x] WiFi 旧配置自动迁移到 SsidManager

### 硬件验证状态 (2026-06-10)

- [x] **小智AI:** 编译通过，已烧录到ota_1分区，IO映射已适配 (BOOT=IO17, LED=IO18)
- [x] **LCD显示:** 2026-06-05 测试通过，ESP-IDF驱动正常，240x280显示正常
- [x] **按键:** OK，7键全部正常（UP/DOWN/LEFT/RIGHT/A/B/START）
- [x] **AHT20传感器:** 温湿度读取正常
- [x] **IMU (LSM6DS3):** **✅ 2026-06-09 修复** — 地址0x6B，WHO_AM_I=0x6A
- [x] **BLE HID:** **✅ 2026-06-10 测试通过** — 键盘+鼠标，Just Works配对
- [x] **空中飞鼠:** **✅ 2026-06-10 测试通过** — 零偏校准+滤波+灵敏度
- [x] **ES8311:** 初始化成功，待音频输出测试
- [x] **ES7210:** 初始化成功，待麦克风测试
- [x] **WiFi:** 连接正常，天气数据获取正常
- [x] **NES模拟器:** 框架初始化成功
- [ ] **电池 ADC:** 待测试（需要接入电池）
- [ ] **SD 卡:** 待测试（需要插入TF卡）
- [ ] **音频输出:** 待测试
- [ ] **WiFi语音流:** 待开发（Phase 2）

### 2026-06-10 Game固件测试记录

**测试环境:** ESP32-S3 (QFN56 rev v0.2), 8MB Octal PSRAM, 16MB Flash, COM3

**新增功能：**
1. ✅ BLE HID键盘+鼠标（Just Works配对，设备名"ESP-Handheld"）
2. ✅ 空中飞鼠（IMU LSM6DS3 @ 0x6B → BLE鼠标）
3. ✅ ⌨ Kbd App（键盘HID模式，A=Enter, B=ESC）
4. ✅ 🖱 Mouse App（空中飞鼠+鼠标键，三级灵敏度）
5. ✅ 菜单6项（NES/Config/Kbd/Mouse/About/Timer）
6. ✅ START+B组合键退出HID模式
7. ✅ 天气修复（gunzip动态缓冲区）
8. ✅ 全英文UI（避开LVGL字体不全问题）

**当前问题：**
- SD卡未插入（ESP_ERR_INVALID_CRC，不影响其他功能）
- 电池ADC可能冲突（已跳过）
- NES颜色可能需要调整

### 2026-06-05 Game固件测试记录

**关键修复:**
1. PSRAM模式: Quad → Octal（与硬件匹配）
2. LCD驱动: 简单4命令 → ESP-IDF完整驱动
3. I2C驱动: 旧驱动 → 新驱动（避免冲突）
4. SPI模式: 0 → 3（gezipai板需要）
5. 按键引脚: 修正为正确GPIO（A=IO17, B=IO16, START=IO15）
6. 任务栈: 2048 → 4096（避免溢出）

**测试结果:**
1. ✅ LCD显示正常（主界面、菜单、状态栏）
2. ✅ 按键功能正常（导航、选择、返回）
3. ✅ AHT20传感器正常（37-38°C, 40-42%湿度）
4. ✅ 音频编解码器初始化成功
5. ✅ BLE/WiFi初始化成功
6. ✅ 天气/时间同步初始化成功
7. ✅ NES模拟器框架初始化成功（SCREENMEMORY和cachedRom分配成功）
8. ⚠️ 电池监控任务会导致系统崩溃（ADC冲突，暂时禁用）
9. ⚠️ IMU未检测到（返回0xFF）

**结论:** Game固件基本功能正常，可以正常运行。需要进一步测试NES模拟器、SD卡、音频等功能。

---

## 小智AI构建指南（踩坑记录）

### 编译环境要求

| 项目 | 版本/路径 |
|------|----------|
| ESP-IDF | v5.5.4 @ `D:\Espressif\frameworks\esp-idf-v5.5.4` |
| Python | 3.11 @ `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env` |
| 工具链 | xtensa-esp-elf 14.2.0 @ `D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121` |
| 源码 | `D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main` |

### 关键配置

| 配置项 | 值 | 说明 |
|--------|-----|------|
| 板型 | gezipai | `CONFIG_BOARD_TYPE_GEZIPAI=y` |
| 目标芯片 | esp32s3 | `CONFIG_IDF_TARGET="esp32s3"` |
| PSRAM | Octal 80MHz | 硬件为Octal模式（非Quad） |
| Flash | 16MB QIO | |
| 分区表 | `partitions.csv` | factory(4M) + ota_0(4M) + ota_1(4M) + assets(4M) |
| 组件管理器 | **必须启用** | `IDF_COMPONENT_MANAGER=1`（首次需联网下载托管组件） |

### gezipai板IO映射（已适配本硬件）

| 功能 | GPIO | 备注 |
|------|------|------|
| BOOT按键（按住说话） | IO17 | 按键A |
| 音量+ | IO5 | UP键，与游戏机共用 |
| 音量- | IO6 | DOWN键，与游戏机共用 |
| LED指示灯 | IO18 | 独立LED，固件中有闪烁逻辑 |
| LCD背光 | IO9 | NPN三极管驱动（反相） |
| LCD DC | IO10 | |
| LCD CS | IO11 | |
| LCD SCLK | IO12 | |
| LCD MOSI | IO13 | |
| LCD RST | IO14 | |
| 音频I2S | IO39/40/45/21/48 | ES8311+ES7210 |
| 音频I2C | IO38/47 | Codec控制 |
| 电池ADC | IO3 | |
| 充电状态 | IO41/42 | |

### 踩坑记录

**1. MSYS2环境不兼容**
- ESP-IDF不支持MSYS2/MINGW环境，必须用cmd.exe运行
- 所有构建脚本（`_build.bat`）需先清除MSYSTEM等环境变量
- 在bash终端中用 `cmd.exe //c "xxx.bat"` 调用

**2. 组件管理器必须启用**
- 小智AI有大量托管组件依赖（`idf_component.yml`中列出了50+个组件）
- `IDF_COMPONENT_MANAGER=0` 会导致缺少 `http.h` 等头文件
- 首次构建需要联网下载组件到 `managed_components/` 目录

**3. build目录路径不匹配**
- 如果源码从其他位置复制过来，build目录中cmake缓存的路径会不一致
- 解决：删除build目录重新构建

**4. CMakeLists.txt空参数bug**
- `main/CMakeLists.txt` 第1114行：`--esp_sr_model_path` 和 `--xiaozhi_fonts_path` 在组件不存在时会传空值
- 需要加 `if()` 条件判断（已修复）

**5. 并行编译间歇性失败**
- Windows下高并行度编译会间歇性出现 `cc1.exe: CreateProcess: No such file or directory`
- 解决：用 `ninja -j2` 限制并行度，或重试（增量编译会自动跳过已完成的文件）

**6. PSRAM模式必须匹配**
- 硬件为Octal模式PSRAM，固件必须配置 `CONFIG_SPIRAM_MODE_OCT=y`
- ESP_BSP的 `sdkconfig.defaults` 配置了Quad模式，与硬件不匹配，无法启动

### 快速构建命令

```bash
# 在 D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main 目录下
_build.bat       # 编译（首次需联网下载组件）
_flash_full.bat  # 擦除全片 + 完整烧录（bootloader+分区表+ota_data+固件）
_flash_fw.bat    # 仅更新固件到factory分区（0x10000）
```

### 串口监视器

```bash
# 查看启动日志
python -c "
import serial, time
ser = serial.Serial('COM7', 115200, timeout=1)
ser.dtr = False; ser.rts = True; time.sleep(0.1); ser.rts = False
start = time.time()
while time.time() - start < 8:
    line = ser.readline()
    if line: print(line.decode('utf-8', errors='replace').rstrip())
ser.close()
"
```

### LCD独立测试程序

`lcd_test/` 目录下有一个最小化LCD测试程序，直接操作SPI和ST7789，不依赖任何框架：
- 依次显示红→绿→蓝→白→彩条
- 背光100%
- SPI 40MHz mode 0
- 用于验证LCD硬件是否正常

### 未来计划

- [ ] **WiFi 音频:** ESP32 采集麦克风 → WiFi → PC 端接收程序
- [ ] **NES OTA:** Game 固件在线升级
- [ ] **小智 AI OTA:** 从 Game 侧管理小智固件升级
- [ ] **随机 Passkey:** BLE 配对码改为随机生成
- [ ] **更多 NES mapper:** 按需扩展兼容性
- [ ] **PC 接收端软件:** 用于 WiFi 音频 + 空中飞鼠

---

## 8. 资源链接

### 组件/库

| 名称 | 链接 | 用途 |
|------|------|------|
| esp32s3_nes_gamer | https://github.com/planevina/esp32s3_nes_gamer | NES 模拟器核心 |
| esp-wifi-connect | https://github.com/78/esp-wifi-connect | WiFi 管理 + 配网 |
| esp_codec_dev | https://components.espressif.com/components/espressif/esp_codec_dev | 音频编解码设备驱动 |
| LVGL v8.4 | https://github.com/lvgl/lvgl | 嵌入式图形库 |
| 小智 AI | https://github.com/78/xiaozhi-esp32 | AI 语音交互固件 |

### 芯片手册

| 器件 | 手册 |
|------|------|
| ESP32-S3 | https://www.espressif.com/en/products/socs/esp32-s3 |
| ST7789 | 通用 SPI LCD 控制器 |
| AHT20 | https://www.micro-semiconductor.com/datasheet/a4e-AHT20.pdf |
| LSM6DS3TR-C | https://www.st.com/en/mems-and-sensors/lsm6ds3tr-c.html |
| ES8311 | https://www.everest-semi.com/product/ES8311.html |
| ES7210 | https://www.everest-semi.com/product/ES7210.html |

### 工具

| 工具 | 说明 |
|------|------|
| ESP-IDF v5.5.4 | 官方开发框架 |
| esptool | Flash 烧录工具 |
| idf.py monitor | 串口监视器 |
| ninja | 构建系统 |

### 服务

| 服务 | 用途 |
|------|------|
| 和风天气 (devapi.qweather.com) | 天气数据 API |
| ntp.aliyun.com | NTP 服务器 |
| pool.ntp.org | NTP 备用服务器 |

---

## 9. 设计决策记录

### 9.1 WiFi 管理方案

**问题:** 需要一个支持多 SSID、自动重连、配网的 WiFi 管理器。

**选项:**
- A. 替换为 `78/esp-wifi-connect` 组件 (参考小智 AI)
- B. 继续使用手写 `wifi_manager` + `wifi_provisioning`
- C. ESP-IDF 原生 `esp_wifi_prov`

**决定:** 选 A。理由: 组件成熟、支持多 SSID (SsidManager)、内置配网 Web 页面 + DNS 劫持。C++ API 通过 `wifi_bridge.cpp` 桥接到 C。

### 9.2 双系统实现方式

**问题:** 需要同时支持 Game 手柄功能和小智 AI 语音交互。

**选项:**
- A. 同分区共存 (LVGL 内嵌小智)
- B. 双固件切换 (分区级)
- C. 纯外部模块 (小智独立硬件)

**决定:** 选 B。理由: 两系统体量大无法共存于一个分区；分区切换是 OTA 原生能力，可靠。Game 占据 factory (4MB)，小智占据 ota_1 (4MB)，通过 `esp_ota_set_boot_partition` + 重启切换。

### 9.3 WiFi 音频必要性

**问题:** BLE 能否替代 WiFi 音频？

**决定:** 不能。BLE 带宽不足以传输高质量音频。WiFi 音频走 UDP/WebSocket 流，PC 端需配套接收程序。此模块编码完成 (半成品)，待硬件验证后再完善。

### 9.4 音频驱动架构

**问题:** ES8311 + ES7210 共享 I2S 总线，如何管理？

**决定:** 参考小智 AI 的 `BoxAudioCodec` 架构。I2S_NUM_0 同时创建 TX+RX 双通道，TX=标准模式(ES8311)，RX=TDM 4 槽(ES7210)。`esp_codec_dev` 按 dev_type 路由音频数据。

### 9.5 小智 AI OTA 策略

**问题:** 小智 AI 使用我们的分区表后，其 OTA 槽指向 ota_0 而不是 ota_1，会写错分区。

**决定:** 小智侧禁用 OTA。更新方式:
1. 构建时使用本项目分区表，使 app 定位在 0x810000
2. 通过 esptool 直接写入 ota_1
3. 后期可改为 Game 侧统一管理 OTA（Game 下载小智固件 → 写入 ota_1）

### 9.6 电源管理

**问题:** 无 MOS 硬件开关，如何省电？

**决定:** 纯软件方案：
- DFS: `esp_pm_configure()` 自动 240MHz↔40MHz 调频
- Light Sleep: 按键唤醒 (GPIO 低电平), ST7789 sleep-in/out
- 活跃时持 `ESP_PM_CPU_FREQ_MAX` lock, 休眠前释放

### 9.7 PC远程控制 - 物理按键模式冲突

**问题:** 7个物理按键（上/下/左/右/A/B/START）既要控制设备UI（导航菜单、调节灵敏度），又要作为PC外设（键盘映射、飞鼠操作），同一套按键无法同时承担两种角色。

**选项:**
- A. 模式切换：设备控制模式 ↔ PC控制模式，通过"开始控制/停止控制"切换
- B. 组合键前缀：按住特定键后其他键才发往PC（如按住START+方向=PC方向键）
- C. 上下文感知：根据当前活跃App自动切换模式
- D. 蓝牙双通道：同时作为BLE键盘和HID设备以不同角色连接

**决定:** 选 A。理由:
1. 组合键方案（B）操作复杂，不符合直觉
2. 上下文感知（C）在需要同时控制设备界面和PC时无法工作
3. 模式切换有明确的进入（"开始控制"按钮）和退出（B键）动作，状态明确
4. 键鼠设置界面作为模式切换前的配置入口，调节灵敏度/按键映射预设后才进入PC控制模式

**设计细节:**
- 默认处于**设备控制模式**，实体按键导航UI（设置界面调节灵敏度、按键映射）
- 按"开始控制"按钮 → 切换到**PC控制模式**
- PC控制模式按键映射:
  - 上/下 → PC ↑/↓ 键
  - 左/右 → PC ←/→ 键
  - A → PC Enter（确认）
  - B → 语音输入（按住采集，松开发送）
  - START/BOOT → 退出PC控制模式
  - BOOT+A → 强制退出PC控制模式（备用方案）
  - 六轴IMU → 空中飞鼠（BLE HID Mouse）
- 此设计已反映在Pencil UI设计文档和 `docs/superpowers/specs/2026-05-16-esp32-s3-handheld-design.md` 中

---

## 10. 硬件调试指南

拿到硬件后按此顺序排查。

### 10.1 电源

1. 测量电池电压 (万用表)
2. 检查 3.3V LDO 输出是否正常
3. 上电串口日志搜索 `Starting...`

### 10.2 I2C 设备探测

连接串口后运行 `idf.py monitor`, 日志会显示各 I2C 设备的初始化结果。

**若失败:** 用 `i2cdetect` 风格扫描（可临时在 `main.c` 或 `esp_log` 中加扫描代码），确认各设备地址是否应答：

| 地址 | 设备 | 总线 |
|------|------|------|
| 0x38 | AHT20 | 传感器 (GPIO 1/2) |
| 0x6A | LSM6DS3TR-C | 传感器 (GPIO 1/2) |
| 0x18 | ES8311 | 音频 (GPIO 38/47) |
| 0x40 | ES7210 | 音频 (GPIO 38/47) |

### 10.3 LCD 显示

1. 启动后会显示 RGB 三色测试图案 (`main.c:show_test_pattern`)
2. 然后 LVGL 接管显示主界面
3. **无显示 / 白屏:** 检查 SPI 连接 (CS/DC/RST/SCLK/MOSI), 背光 PWM (GPIO 9), 复位时序

### 10.4 按键

1. 按下任意键, 串口应有 `KEY_DRV` 日志
2. **无反应:** 检查 GPIO 电平（无按键时应为高电平）
3. **连击 / 抖动:** 检查硬件上拉, 可调整 `key_driver.c` 消抖参数

### 10.5 电池/ADC

1. 串口日志每 10 秒输出 `Battery: XmV X% (charging=X charged=X)`
2. 对比万用表实测电压
3. 若电压偏差大: 检查分压电阻 (1:2), ADC 参考电压 (3.3V), eFuse 校准
4. CHRG (GPIO 41) 和 DONE (GPIO 42) 接充电芯片对应引脚

### 10.6 BLE

1. 启动后菜单进入 "PC远控" → 显示广播状态和 Passkey
2. PC 端搜索 BLE 设备 "ESP-Handheld" 配对
3. Passkey 默认 "1234"
4. **失败:** 检查 Bluedroid 配置 (sdkconfig), 天线匹配

### 10.7 音频

1. **ES8311:** 日志 `ES8311 not detected` = I2C 失败。检查 GPIO 38/47 连接
2. **ES7210:** 日志 `ES7210 not detected` 类似
3. **I2S:** MCLK/BCLK/WS 用示波器看时序（256×16kHz=4.096MHz MCLK）
4. **PA 使能:** ES8311 PA 由 GPIO 46 控制，需拉高才有声音输出

### 10.8 SD 卡

1. FATFS 挂载日志可见 `/sdcard` 是否成功
2. NES ROM 放 `/sdcard/roms/*.nes`
3. **失败:** SPI 模式兼容性, 检查 CS (GPIO 8), 换卡测试

### 10.9 NES 模拟器

1. SD 卡 → ROM 浏览器 → 选择 ROM
2. B + START 退出
3. **卡死:** 检查 PSRAM, NES 需要大量 RAM
4. **无画面:** 检查 NES→LCD 帧缓冲传递

### 10.10 双系统切换

1. 菜单 → AI → 若 ota_1 为空 → 日志 `Target partition not found`, 不会重启
2. 确认 ota_1 已烧录有效固件
3. 切换后重启观察日志确认从正确分区启动

### 快速故障排查表

| 现象 | 检查 |
|------|------|
| 无任何串口输出 | 电源、USB 转串口、波特率 115200 |
| 只输出 bootloader 日志 | 固件损坏, 重新烧录 |
| LCD 白屏 | SPI 线序、背光 PWM、复位 |
| I2C 设备 NACK | 接线、上拉电阻、地址正确性 |
| BLE 搜不到 | 天线、Bluedroid 使能、广播配置 |
| NES 卡死 | PSRAM 分配失败、ROM 不兼容的 mapper |

---

> **最后提醒:** 硬件到手后先跑一遍快速排查表。每个模块初始化都有日志输出，大部分问题可以定位到具体模块。调试时优先确保 I2C 总线 OK（传感器+音频），这是最多模块依赖的基础设施。
