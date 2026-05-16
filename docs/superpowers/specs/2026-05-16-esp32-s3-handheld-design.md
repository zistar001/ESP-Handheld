# ESP32-S3 智能小手柄 设计文档

**日期:** 2026-05-16
**项目:** ESP32-S3 手持游戏手柄 (ESP_BSP)
**框架:** ESP-IDF + ESP-BSP + LVGL + FreeRTOS

---

## 1. 硬件配置

### 1.1 主控
- ESP32-S3 模组，双核 Xtensa LX7 @240MHz
- PSRAM（用于ROM缓存、语音缓冲）

### 1.2 外设与引脚映射

| 外设 | 接口 | 引脚 |
|------|------|------|
| ST7789 LCD (1.69") | SPI | BL=IO9, DC=IO10, CS=IO11, SCK=IO12, MOSI=IO13, RES=IO14 |
| TF 卡 | SPI (共用 SPI 总线) | CMD=IO13, CLK=IO12, DATA=IO8 |
| IMU LSM6DS3TR-C | I2C | SDA=IO2, SCL=IO1 |
| AHT20 温湿度 | I2C (共用总线) | SDA=IO2, SCL=IO1 |
| 喇叭 ES8311 | I2S 输出 | VOP=IO3, VON=IO46 |
| 麦克风 ES7210 | I2S 输入 | IN1=IO37-38, IN2=IO39-40 |
| 方向键: 上/下/左/右 | GPIO 输入 | IO5/IO6/IO4/IO7 |
| 功能键: A/B/START | GPIO 输入 | IO16/IO15/IO17 |
| 物理电源开关 | 硬件断电 | - |

### 1.3 注意事项

- LCD 与 TF 卡共用 SPI 总线(IO12, IO13)，通过不同 CS 片选区分
- IMU 与 AHT20 共用 I2C 总线(IO1, IO2)，地址不同，无冲突
- 物理电源开关直接切断电池供电，不依赖软件电源管理

---

## 2. 软件架构

### 2.1 层级结构

```
┌──────────────────────────────────────────────┐
│              应用层 (Apps)                      │
│  Launcher | Menu | NES | 小智AI | PC远程 | 设置 │
├──────────────────────────────────────────────┤
│             应用管理器 (App Manager)             │
│    状态机: LAUNCHER → MENU → APP_RUNNING       │
├──────────────────────────────────────────────┤
│            LVGL 图形界面 (UI Layer)              │
│      screens/ components/ fonts/ animations    │
├──────────────────────────────────────────────┤
│            FreeRTOS 多任务调度                   │
│  Core 0: UI/传感器/管理   Core 1: 模块运算       │
├──────────────────────────────────────────────┤
│          BSP 层 (板级支持包)                     │
│  自定义BSP: LCD/SD/IMU/Audio/Key 驱动初始化     │
├──────────────────────────────────────────────┤
│          ESP-IDF 底层 (HAL/Driver)              │
└──────────────────────────────────────────────┘
```

### 2.2 双核任务分配

**ESP32-S3 是双核处理器，两个核心同时运行 @240MHz，共享所有内存资源。** 核心分配是给 FreeRTOS 调度器的 task affinity 建议，不是"锁死"——当一个模块运行时，两个核心都可用。

Core 0 (Pro CPU):
- LVGL 渲染与刷新 (60fps)
- 按键扫描与软件去抖
- AHT20 温湿度采样
- 应用管理器状态流转
- WiFi/BT 协议栈任务

Core 1 (App CPU):
- NES 模拟器核心逻辑
- 小智 AI 语音处理 (ASR/TTS)
- 音频编解码 (Opus 编码/解码)
- UDP 语音数据收发

---

## 3. 应用管理

### 3.1 状态机

```
        按START           方向键+按A
LAUNCHER ──────→ MENU ──────────────→ APP_RUNNING
  ↑                ↑                      │
  └──── 按START ───┘   按B键 ──────────────┘
```

### 3.2 应用接口

```c
typedef struct {
    const char *name;           // 模块名称
    lv_obj_t *(*init)(void);    // 初始化 → 返回LVGL容器对象
    void (*run)(void);          // 启动模块逻辑(创建任务)
    void (*stop)(void);         // 停止模块(销毁任务+释放资源)
    void (*deinit)(void);       // 反初始化
} app_module_t;
```

### 3.3 按键映射

| 物理按键 | Launcher | Menu | App Running |
|---------|---------|------|-------------|
| 上/下/左/右 | - | 图标导航 | 各App自处理 |
| A | - | 确认进入 | 各App自处理 |
| B | - | 返回上一级 | 返回菜单 |
| START | 进入菜单 | 返回Launcher | 返回Launcher |

---

## 4. 功能模块

### 4.1 主界面 (Launcher)

布局:
- 顶部状态栏: 音量/亮度/WiFi/BLE 图标
- 中央: 大字体时间 + 日期
- 天气信息: 温度/天气图标
- 设备信息: 内部温度/湿度
- 底部: "按START进入菜单"

数据源:
- 时间/日期: NTP 网络对时（有WiFi时），或 RTC
- 天气: 通过 WiFi 请求天气 API
- 设备温度/湿度: AHT20 传感器

### 4.2 菜单界面 (Menu)

- 九宫格图标布局，方向键移动高亮框
- A 键确认，B 键返回，START 回 Launcher
- 功能项: NES模拟器、小智AI、PC远程控制、设置、关于

### 4.3 NES 模拟器

- 来源: `esp32s3_nes_gamer` (https://github.com/planevina/esp32s3_nes_gamer)
- ROM: TF 卡加载，PSRAM 缓存
- 显示: 直接写入显示缓冲区，60fps
- 按键: 上/下/左/右/A/B/START 直映射
- 退出: START + B 组合键
- 与 小智AI 互斥

### 4.4 小智 AI 语音助理

- 移植自 `D:\code\XIAOZHI_AI\xiaozhi-esp32-main\main`
- WebSocket 连接云端 AI 服务
- 音频链: ES7210 → AEC/降噪/VAD → ASR → AI → TTS → ES8311
- 大块 PSRAM 给语音缓冲
- 与 NES 互斥

### 4.5 PC 远程控制 (pc_remote)

融合 BLE 键鼠 + 空中飞鼠 + WiFi 语音输入:

**连接:**
- BLE HID: 常连接 PC，传输键鼠指令
- WiFi UDP: 同一局域网，按需传输语音

**三种子模式 (可切换):**
1. BLE 键盘: 7键自定义 (默认: 方向 + Enter/Esc/Ctrl+C)
2. 空中飞鼠: IMU → BLE HID Mouse
3. BLE 游戏手柄: HID Gamepad

**语音输入流程:**
```
按住B键 → I2S采集 → Opus编码 → WiFi UDP → PC接收服务 → Opus解码 → 虚拟声卡 → PC语音输入法 → 文字输入
松开B键 → 停止发送
```

**PC 端服务:** UDP 监听 → Opus 解码 → 虚拟音频设备输出

### 4.6 设置模块

- 音量调节 (ES8311 增益)
- 屏幕亮度 (PWM)
- WiFi 连接
- BLE 配对
- 按键自定义 (PC远程模式)
- 关于信息

---

## 5. 自定义 BSP

基于 ESP-BSP 框架，`bsp_board.c/h` 适配当前引脚:

- LCD: ST7789 SPI + 背光 PWM
- SD: SPI 模式 FAT 挂载
- IMU: LSM6DS3TR-C I2C 初始化 + 数据读取
- AHT20: I2C 温湿度
- Audio: ES8311 I2C+I2S 输出 + ES7210 I2C+I2S 输入
- Key: 7 GPIO + 软件去抖

---

## 6. 开发路线图

| 阶段 | 内容 | 里程碑 |
|------|------|--------|
| 1 | IDF + BSP + 引脚适配 + 点亮屏幕 | 屏幕显示测试图案 |
| 2 | 按键 + LVGL 主界面/菜单 | 按键导航正常 |
| 3 | IMU + AHT20 驱动 | 传感器数据正确 |
| 4 | TF 卡 FAT 挂载 | 文件读写正常 |
| 5 | NES 模拟器集成 | 游戏运行流畅 |
| 6 | BLE HID 键鼠 + 空中飞鼠 | PC 识别手柄输入 |
| 7 | WiFi 语音 + PC 端服务 | 语音输入到电脑 |
| 8 | 小智 AI 移植 | 语音交互正常 |
| 9 | 设置模块 + 全功能集成 | 功能切换正常 |
| 10 | 电源管理 (按需) | 待机功耗达标 |

---

## 7. 项目目录结构

```
main/
├── main.c                     # 入口
├── app/
│   ├── app_manager.c/h        # 应用状态机
│   ├── launcher.c/h           # 主界面
│   └── menu.c/h               # 菜单
├── bsp/
│   └── bsp_board.c/h          # 自定义BSP
├── modules/
│   ├── nes/                   # NES模拟器
│   ├── xiaozhi/               # 小智AI (移植)
│   ├── pc_remote/             # PC远程控制
│   ├── imu/                   # IMU驱动
│   └── sensor/                # AHT20驱动
├── ui/
│   ├── screens/
│   │   ├── main_screen.c/h
│   │   ├── menu_screen.c/h
│   │   └── settings_screen.c/h
│   └── components/
└── CMakeLists.txt
```
