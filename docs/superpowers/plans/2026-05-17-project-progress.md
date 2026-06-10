# ESP32-S3 智能小手柄 — 项目进度与待办清单

**更新日期:** 2026-05-17
**构建状态:** ✅ 编译通过（0 warning, 0 error）
**代码审查:** ✅ 已通过（14 issues 全部修复）

**参考文档:**
- [项目参考手册](../project-reference.md) — 完整的架构、模块、引脚、调试指南
- [刷机指南](../flash-guide.md) — 双系统构建与烧录步骤
- [设计规格](../specs/) — 各功能设计文档

---

## 概况

| 模块 | 状态 | 完成度 |
|------|------|--------|
| ✅ 硬件抽象层 (BSP) | 完整 | 100% |
| ✅ LVGL 图形界面 | 已集成 | 95% |
| ✅ NES 模拟器 | 完整 | 95% |
| ✅ 按键驱动 | 完整 | 100% |
| ✅ 传感器 (AHT20/IMU) | 完整 | 100% |
| ✅ BLE HID 键鼠 | 编码完成 + 配对码屏幕显示 | 95% |
| ✅ 空中飞鼠 | 陀螺仪零偏校准 + 低通滤波 + 死区 | 90% |
| ✅ 设置管理 | 界面完整 | 90% |
| ✅ 天气 (和风 API 泉州) | 已集成, 使用共享 WiFi | 95% |
| ✅ ROM 浏览器 | SD卡扫描 *.nes | 100% |
| ✅ IMU 姿态显示 | 主界面显示 | 100% |
| ✅ 状态栏 | WiFi/BLE/音量/亮度/电量 | 100% |
| ✅ ES8311 音频驱动 | 编码完成 | 80% |
| ✅ 电源管理 (软件) | DFS + Light Sleep | 80% |
| ✅ 电池/ADC | ADC 读取 + 充电检测 + 状态栏显示 | 80% |
| ✅ WiFi (esp-wifi-connect 组件) | 共享 STA + 多 SSID + Soft-AP 配网 | 100% |
| ✅ RTC (SNTP) | NTP 同步 + NVS 备份 + 时区 UTC+8 | 90% |
| ✅ 双系统切换 | factory ↔ ota_1 分区切换 | 100% |
| ❌ 小智 AI 固件 | 独立项目, 烧录到 ota_1 | 0% |
| ❌ 硬件验证 | 未做 | 0% |

---

## 一、硬件抽象层 (BSP) — 100%

**文件:** `main/bsp/bsp_board.c`, `st7789_driver.c/.h`, `sd_card.c/.h`

- [x] ST7789 SPI 驱动（60MHz, DMA, sleep in/out）
- [x] SD 卡 SPI 挂载 FATFS（`/sdcard`）
- [x] 外部传感器 I2C 总线（GPIO 1/2, 100KHz）
- [x] 背光 PWM（LEDC, 13bit）
- [x] 按键消抖扫描（3 次采样, 10ms 间隔）
- [x] 音频 Codec I2C 总线（GPIO 38/47, 100KHz）— 模组内部，与传感器独立
- [x] 音频 I2S 共享总线（MCLK=40, BCLK=39, WS=45, DOUT=48, DIN=21）— 模组内部 ES8311+ES7210
- [x] ES8311 PA 使能: GPIO 46

**⚠️ 注意引脚层级:** 模组内部音频引脚 (GPIO 21/38/39/40/45/46/47/48) 与板级外设引脚 (传感器 I2C GPIO 1/2, LCD, SD, 按键) 是分开的，不要混淆。板级 MIC 接口 (IO37-40) 是 ES7210 的模拟输入，不是 ESP32 GPIO。

---

## 二、LVGL 图形界面 — 95%

**文件:** `ui/display_driver.c`, `ui/screens/*.c`, `ui/components/*.c`

- [x] LVGL v8.4 集成, PSRAM 40 行缓冲
- [x] 主界面: 时间/温度/天气/IMU/提示
- [x] 状态栏: WiFi/BLE/音量/亮度图标
- [x] 菜单: 3 列网格, 焦点高亮
- [x] 设置页面: 全部 9 字段（滚动）
- [x] NES 运行时暂停 LVGL
- [x] 关于页面: 版本/构建信息
- [x] ROM 浏览器: 扫描 SD 卡 *.nes 列表

---

## 三、NES 模拟器 — 95%

**文件:** `app/rom_browser.c`, `modules/nes/*`

- [x] 6502 CPU 完整指令集, PPU 渲染
- [x] 44 种 MMC mapper
- [x] 7 键映射 + B+START 退出
- [x] PSRAM ROM 缓存
- [x] ROM 浏览器: 扫描 `/sdcard/roms/*.nes` 列表选取

**缺失:**
- [ ] ES8311 音频输出待硬件验证
- [ ] 未测试 mapper 兼容性

---

## 四、应用管理 & 按键 — 100%

**文件:** `app/app_manager.c`, `app/launcher.c`, `app/menu.c`, `app/rom_browser.c`

- [x] 三态状态机: LAUNCHER → MENU → RUNNING
- [x] 5 个 App 入口 + ROM 浏览器
- [x] APP_ID_ABOUT — 显示版本信息
- [x] APP_ID_PC_REMOTE — BLE HID 状态屏
- [x] APP_ID_NES — ROM 浏览器
- [x] 按键消抖 + 回调分发 + NES 直通

---

## 五、传感器 — 100%

**文件:** `modules/sensor/aht20_driver.c/.h`, `modules/imu/imu_driver.c/.h`

- [x] AHT20 每 5 秒读取, 显示在主界面
- [x] IMU 六轴, 10ms 读取, Pitch/Roll 显示在主界面
- [x] IMU 数据用于空中飞鼠

---

## 六、BLE 键鼠 — 95%

**文件:** `modules/pc_remote/ble_hid.c/.h`

- [x] Bluedroid BLE, HID 键盘+鼠标
- [x] 安全配置 + 广播 + 断线重连
- [x] 与 app_manager 联动

**问题:**
- [ ] passkey 硬编码 1234
- [ ] 未在真实 PC 上测试

---

## 七、天气模块 (和风 API) — 95%

**文件:** `modules/weather/weather.c/.h`

- [x] WiFi STA 自动连接
- [x] HTTPS 调用 `devapi.qweather.com/v7/weather/now`
- [x] cJSON 解析 + 主界面显示
- [x] 配置: 泉州 (101230501)
- [x] 后台每 30 分钟自动更新

---

## 八、共享 WiFi 管理器 + Soft-AP 配网 — 100%

**文件:** `modules/wifi_manager/wifi_manager.c/.h` (C API), `modules/wifi_manager/wifi_bridge.cpp` (C++ 桥接)

参考小智 AI 的 [`78/esp-wifi-connect`](https://github.com/78/esp-wifi-connect) 组件 v3.1.x 重构。组件提供 WifiManager 单例 + SsidManager 多 SSID 管理 + 内置配网页面。

**组件托管方式:** 克隆到 `components/esp-wifi-connect/`，本地组件无需 IDF 组件管理器。

**WiFi STA 管理:**
- [x] `WifiManager` 单例初始化，幂等安全多次调用
- [x] 事件回调: Connected / Disconnected / ConfigModeEnter / ConfigModeExit
- [x] `wifi_manager_init()` → `wifi_manager_wait_connected()` 供各模块使用
- [x] 自动重连 + 指数退避扫描

**Soft-AP 配网:**
- [x] 内置 Web 配置页 + DNS 域名劫持（组件自带）
- [x] `wifi_manager_start_config()` / `stop_config()` / `is_config_mode()`
- [x] `wifi_manager_has_saved_ssid()` — 检查是否已配网
- [x] 多 SSID 存储 (SsidManager, NVS 命名空间 "wifi")
- [x] 旧版 `settings_manager` 数据自动迁移到 SsidManager
- [x] 首次启动自动进入配网模式
- [x] 菜单入口 "WiFi设置" (APP_ID_WIFI_SETUP)
- [x] 配网完成后自动重启

**遗留桩文件:** `wifi_provisioning.c/.h` 保留为转发桩，向后兼容。

**用法:** 各模块调用 `wifi_manager_init()`，底层由 WifiManager 处理。

**文件:** `modules/time_sync/time_sync.c/.h`

由于 ESP32-S3 无独立硬件 RTC，使用 SNTP 网络对时 + NVS 备份：

- [x] SNTP 客户端: `ntp.aliyun.com` + `pool.ntp.org`
- [x] 时区: CST-8 (UTC+8, 中国标准时间)
- [x] NVS 存储最后同步时间，重启时恢复 (避免首次启动显示 --:--)
- [x] `settimeofday` 更新系统时间，后续由 ESP-IDF 内部定时器维持
- [x] 主界面时间/日期替换原来的伪时钟 (uptime 模拟)

**局限性:**
- [ ] 首次启动若无 NVS 备份且 WiFi 未就绪，显示 `--:--`
- [ ] SNTP 需要 WiFi 连接，同步前显示 NVS 备份时间（可能不精确）

**文件:** `modules/settings/settings_manager.c`, `ui/screens/settings_screen.c`

- [x] NVS 读写, 9 字段结构体
- [x] settings_init 首次启动创建命名空间
- [x] 设置 UI: 音量/亮度/麦克风增益/鼠标灵敏度/休眠超时/休眠开关/WiFi状态/BLE状态
- [x] 音量滑块实时更新 ES8311 音频 Codec

---

## 十、ES8311 音频输出 + ES7210 麦克风输入 — 90% (编码完成)

**文件:** `modules/audio/box_audio_codec.c/.h` (核心), `modules/audio/es8311_driver.c/.h` (薄封装), `modules/audio/es7210_driver.c/.h` (薄封装)
**参考:** 小智 AI 项目的 `BoxAudioCodec` (C++ → C 移植)
**依赖:** `espressif/esp_codec_dev` v1.5.6 (components/espressif__esp_codec_dev)

### 硬件架构

ESP32-S3 模组**内部集成** ES8311 (DAC) + ES7210 (ADC)，共享同一组 I2S 总线：

| 信号 | GPIO | 连接 |
|------|------|------|
| MCLK | 40 | 共享时钟 (256× fs = 4.096MHz @ 16kHz) |
| BCLK | 39 | 共享位时钟 |
| WS   | 45 | 共享字选择 |
| DOUT | 48 | ESP32 → ES8311 DIN (音频输出) |
| DIN  | 21 | ES7210 DOUT → ESP32 (麦克风输入) |
| PA   | 46 | ES8311 功放使能 |
| I2C SDA | 38 | 音频编解码器控制总线 (与传感器 I2C 独立) |
| I2C SCL | 47 | 音频编解码器控制总线 |

**板级 MIC 接口**（IO37~40）是 ES7210 的**模拟输入端**，非 ESP32 GPIO：
- MIC1_IN → ES7210 通道 1
- MIC1_IP → ES7210 通道 1+
- MIC2_IN → ES7210 通道 2
- MIC2_IP → ES7210 通道 2+

**驱动架构 (BoxAudioCodec 模式):**
- I2S_NUM_0 创建 TX+RX 双通道: TX=标准模式(ES8311), RX=TDM 4 槽(ES7210)
- 共用 `audio_codec_new_i2s_data()` 接口，`esp_codec_dev` 按 dev_type 路由
- I2C 控制总线独立于传感器 I2C (GPIO 1/2)
- ES8311 地址: `ES8311_CODEC_DEFAULT_ADDR` (0x30, 8-bit → 0x18 7-bit)
- ES7210 地址: `ES7210_CODEC_DEFAULT_ADDR` (0x80, 8-bit → 0x40 7-bit)
- 采样率: 16kHz, 位深: 16-bit
- ES7210 选择 MIC1+MIC2，4 通道 TDM 模式
- 音量 0-100, 与设置页面联动

**重要: 不要用 `BSP_I2S_BCK` `BSP_MIC_BCK` 等旧定义 — 它们已被删除。使用 `BSP_AUDIO_BCLK` `BSP_AUDIO_WS` 等新定义。**

### APIs
- `box_audio_init()` — 初始化整个音频子系统（I2S + 两个 codec）
- `box_audio_read()` — 从 ES7210 读取 PCM（4 通道 TDM）
- `box_audio_write()` — 向 ES8311 写入 PCM（单声道）
- `box_audio_set_volume()` / `box_audio_set_mic_gain()`
- `es8311_*()` / `es7210_*()` 均为 `box_audio_*()` 的薄封装

**待硬件验证:**
- [ ] I2C 探测 ES8311 和 ES7210 是否应答 (地址 0x18/0x40)
- [ ] I2S MCLK/BCLK/WS 时序 (共用总线)
- [ ] ES7210 TDM 4 槽双麦数据读取
- [ ] 音频输出音质 + 麦克风输入增益
- [ ] 回声消除 (AEC) 效果验证

---

## 十一、电源管理 (软件) — 80%

**方式:** 纯软件方案 (无独立 MOS 硬件)

**已实现:**
- [x] **Light Sleep** — 已有: 按键唤醒, ST7789 sleep in/out
- [x] **DFS (Dynamic Frequency Scaling)** — `esp_pm_configure()`: 240MHz↔40MHz 自动调频
- [x] **Max-freq lock** — 活跃时保持高频, 休眠前释放
- [x] **外设休眠** — ST7789 sleep in/out, I2S 停用

**设计:**
- `CONFIG_PM_ENABLE=y` 启用 DFS
- 休眠超时从设置读取 (默认 300s)
- `esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX)` 控制频率
- 无 MOS 硬件 — 全软件电源优化

---

## 十二、电池/ADC 监控 — 80% (编码完成)

**文件:** `main/modules/power/battery_monitor.c/.h`

**引脚:**
- ADC 检测: GPIO 3 (ADC1_CHANNEL_2, 12bit, 3.3V 参考, 1:2 分压)
- CHRG: GPIO 41 (低电平=充电中)
- DONE: GPIO 42 (高电平=已充满)

**设计:**
- adc_oneshot 单次采样, 10 次平均去抖
- 曲线拟合校准 (eFuse), 无校准则 3300/4095 近似
- 分压比 1:2 → 实际电压 = ADC 电压 × 3
- 百分比: 3000mV~4200mV 线性映射到 0~100%
- 充电状态: CHRG=0 + DONE=1 → 充电中; 两者都高 → 已充满
- 每 10 秒读取一次, 状态栏显示 "⚡72%" 或 "52%"

**待硬件验证:**
- [ ] ADC 实际读数与万用表对比
- [ ] 校准精度 (eFuse 是否已烧录)
- [ ] CHRG/DONE GPIO 逻辑验证

---

## 十三、需要硬件继续的模块

| 模块 | 当前状态 | 依赖 |
|------|---------|------|
| ES7210 麦克风输入 (I2S TDM) | 驱动参考 BoxAudioCodec 可用 | 硬件验证 |
| 小智 AI 融合 | 5% — 空桩 | 硬件 + OTA |
| RTC | 90% — SNTP + NVS 备份完成 | 硬件验证 (时间保持精度) |
| WiFi 语音传输 | 40% — 半成品 | 硬件 + PC端配套 |
| 硬件验证 / 时序测试 | 0% | 硬件 |

---

## 十四、代码审查 — 已完成

**审查日期:** 2026-05-17 | **结果:** 所有问题已修复并编译通过

| 严重度 | 数量 | 已修复 |
|--------|------|--------|
| CRITICAL | 2 | ✅ cJSON NULL 解引用, strncpy 缺终止符 |
| HIGH | 5 | ✅ weather.c 空响应, main.c 死代码/缺失 return, rom_browser 截断警告, g_settings 不同步 |
| MEDIUM | 7 | ✅ 未使用变量, include 多余, rom_browser 路径构建改用 pragma, 其他 |
| LOW | 0 | — |

---

## 双系统架构 — 100%

**设计:** 分区级双系统，通过 OTA 分区切换实现

| 系统 | 分区 | 说明 |
|------|------|------|
| Game (ESP_BSP) | factory (0x10000, 4MB) | 手柄主界面、NES、BLE 远控、天气 |
| Game OTA 升级 | ota_0 (0x410000, 4MB) | 保留用于 Game 固件 OTA 更新 |
| XiaoZhi AI | ota_1 (0x810000, 4MB) | AI 语音交互（独立项目构建） |
| 资源 | assets (0xC10000, ~4MB) | SPIFFS 共享资源 |

**切换逻辑 (`app_manager.c` APP_ID_XIAOZHI):**
- 检测当前运行分区 (`esp_ota_get_running_partition()`)
- 从 factory/ota_0 → 切换到 ota_1 (XiaoZhi)
- 从 ota_1 → 切换回 factory (Game)
- 调用 `esp_ota_set_boot_partition()` + `esp_restart()` 重启

**关于页面:** 动态显示当前系统名称和分区标签

**刷机指南:** 详见 [flash-guide.md](../flash-guide.md)

**依赖:** 需先构建 XiaoZhi AI 固件并烧录到 ota_1 分区

**⚠️ 重要约束:** XiaoZhi AI 固件必须使用与本项目**相同的分区表** (partitions.csv) 来编译，使其 app 分区定位在 0x810000。不能直接拿通用 XiaoZhi 固件（默认编译到 0x10000）烧到 0x810000，因为固件是位置相关的。

---

- [x] 16MB: factory(4M)+ota_0(4M)+ota_1(4M)+assets(4M)
- [x] PSRAM Quad, Bluedroid BLE, FATFS LFN
- [x] 构建通过, 0 warning, 0 error
- [x] 固件大小: ~1.84MB (分区剩余 55%)
