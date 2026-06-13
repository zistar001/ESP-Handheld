# AGENTS.md

## What This Is

ESP32-S3 handheld device firmware — NES emulator + LVGL UI + weather + BLE remote + WiFi audio + dual-firmware with XiaoZhi AI + I Ching fortune divination. ESP-IDF v5.5.4 C project.

Entry point: `main/main.c`. Project name: `esp_handheld`.

## Build & Flash (Read This First)

**Never build from MSYS2/Mingw bash.** ESP-IDF detects the `MSYSTEM` env var and refuses to run. Use cmd.exe or PowerShell only.

### PowerShell build (canonical)

```powershell
$env:MSYSTEM=''
$env:IDF_PATH='D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH='D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='1'
$env:PATH='<cmake>;<ninja>;<xtensa-esp-elf\bin>;<python\Scripts>;' + $env:PATH
python D:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py build
```

### Single flash command (must include ota_data_initial.bin)

```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

**If you skip 0xd000 (ota_data_initial.bin), the device boots into XiaoZhi AI (ota_1) instead of factory.**

### Serial

- Monitor: `idf.py -p COM7 monitor` (115200 baud)
- Flash baud: 921600

## Environment

| Variable | Value |
|----------|-------|
| IDF_PATH | `D:\Espressif\frameworks\esp-idf-v5.5.4` |
| IDF_TOOLS_PATH | `D:\Espressif\tools` |
| Python | `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe` |
| Serial | COM7 |
| Target | esp32s3 |

## Critical Gotchas

### sdkconfig & build directory

- **Never delete `build/` without backing up `sdkconfig`** — the root sdkconfig has LVGL fonts, partition scheme, PSRAM mode that `sdkconfig.defaults` alone doesn't restore.
- `idf.py set-target esp32s3` regenerates from `sdkconfig.defaults`, which is incomplete.
- `managed_components/` is auto-generated — do not commit. `dependencies.lock` should be committed.

### PSRAM memory

- Use `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (default). **Do NOT set `CONFIG_SPIRAM_USE_MALLOC=y`** — it causes DMA components (mbedTLS, I2S, WiFi) to allocate from PSRAM and crash.
- NES `SCREENMEMORY` (61KB) must use `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, not `MALLOC_CAP_DMA`.

### LVGL thread safety

- LVGL is **NOT thread-safe**. All LVGL calls must go through `lvgl_lock()` / `lvgl_unlock()` (defined in `display_driver.c`).
- Every `lv_obj_create(NULL)` (new screen) needs a corresponding `lv_obj_del(old_scr)` or screens leak and exhaust the 64KB LVGL heap.
- Display uses **double-buffer** (`display_driver.c`) — DMA sends buffer A while LVGL renders to buffer B. Single buffer causes horizontal stripes.

### NES emulator

- Runs on **Core 1** (game_task). Video rendering on **Core 0** (video_task).
- Game logic uses fire-and-forget: notifies video_task, doesn't wait for SPI transfer. This keeps game at 60fps while display runs at ~38fps (SPI bottleneck).
- PSRAM shadow buffer prevents tearing: game_task writes SCREENMEMORY, video_task reads s_shadow.
- `nes_game_set_exit_callback()` must be called in `nes_start()` — the callback is set to NULL after each use.
- Task WDT on Core 1 is expected (spin-wait starves idle task). `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n` suppresses it.

### SPI bus sharing

- SD card and LCD share SPI bus. SD initializes first at 400kHz, LCD joins later at 60MHz (actual ~40MHz on ESP32-S3).

### zlib / Component Manager

- `IDF_COMPONENT_MANAGER=1` is required for the zlib dependency (`espressif/zlib: ^1.3.1` in `main/idf_component.yml`).
- Weather API always returns gzip regardless of `&gzip=n` param. Use `inflateInit2(&strm, 16 + MAX_WBITS)` with windowBits=31 for auto-detection.

### Dual-firmware

- Custom `partitions.csv`: factory (0x10000) = ESP_BSP, ota_1 (0x810000) = XiaoZhi AI.
- System switching via `esp_ota_set_boot_partition()` + `esp_restart()`.
- XiaoZhi firmware is position-dependent — must compile with the same `partitions.csv`.

## Project Structure

```
main/
  main.c                    — Entry point, key handler, app init
  app/                      — App framework (app_manager, launcher, menu, rom_browser)
  bsp/                      — Board support (st7789, sd_card, bsp_board)
  modules/
    nes/                    — NES emulator (nes_game, nes_wrapper)
    audio/                  — ES8311 codec, box_audio
    imu/                    — LSM6DS3TR-C IMU driver
    weather/                — HeFeng weather API
    pc_remote/              — BLE HID, air mouse, WiFi audio
    wifi_manager/           — WiFi connect/provisioning
    xiaozhi/                — XiaoZhi AI wrapper
    power/                  — Battery monitor
    settings/               — NVS settings
    time_sync/              — SNTP
    iching/                 — I Ching + Liu Ren (iching_data.c/h, liuren_core.c/h)
  ui/
    display_driver.c        — LVGL init, double-buffer, mutex
    screens/                — home, menu, settings, countdown, airmouse, kbd, ip_input
    components/             — status_bar
components/
  lvgl/                     — LVGL library (local, not from registry)
  lv_conf.h                 — LVGL configuration
  nes_core/                 — NES emulator core (nofrendo-derived)
  esp-wifi-connect/         — WiFi provisioning component
  espressif__esp_codec_dev/ — Codec device abstraction
```

## Code Conventions

- C99 for most code, C++ only for `wifi_bridge.cpp` and `emucore.cpp`.
- GCC 14+ requires `%zu` for `size_t`, explicit `#include <cstring>` in C++.
- Component dependencies are in `main/CMakeLists.txt` REQUIRES list — add new modules there.
- `#include` paths use the INCLUDE_DIRS listed in `main/CMakeLists.txt` (relative to `main/`).

## I Ching Fortune Module

### How It Works

1. Menu → 运势 → 6 项选择（运势/事业/经商/求名/婚恋/决策）
2. 选择后进入摇卦界面，IMU 自动检测上下摇动
3. 强烈摇晃 3+ 次 = 生成一爻（随机阳/阴）
4. 从下到上依次显示，共6爻完成卦象
5. 根据 `YI64.md` 数据计算卦名、卦辞、大象、所选事项解读

### 文件结构

| 文件 | 说明 |
|------|------|
| `main/modules/iching/iching_data.h` | 卦象数据结构 |
| `main/modules/iching/iching_data.c` | 64卦完整数据（从YI64.md生成） |
| `main/ui/screens/fortune_screen.c/h` | 选择→摇卦→结果显示 UI |
| `YI64.md` | 64卦原文数据源 |

### 关键参数

- 摇动阈值：Y轴加速度偏离1g > 0.6g 算一次摇晃
- 摇动冷却：每爻间600ms不接收新摇晃
- 生成新卦象后可滚动查看完整卦辞（↑↓键）
- B/START 返回

### 生成卦象数据

```bash
python gen_iching.py   # 从 YI64.md 重新生成 iching_data.c
```

需要在 `main/CMakeLists.txt` 中 `REQUIRES` 添加 `iching`，`INCLUDE_DIRS` 添加 `modules/iching`。

## 小六壬掌诀模块

### 原理

小六壬将"大安、留连、速喜、赤口、小吉、空亡"六种卦象固定于左手六指节。推算方法：从寅位大安起正月，数至当月；从当月起初一，数至当日；从当日起子时，数至当前时辰。最终落位即为结果。

### 如何使用

1. 菜单 → 小六壬 → 进入手掌界面
2. 按 **A** 键触发"掐指一算"
3. 6神指节依次点亮（白色→杏黄色），循环3轮后停在结果位置
4. 底部显示农历日期、时辰、卦名、断辞
5. 按 **↑↓** 滚动查看完整断辞
6. **B/START** 返回

### 文件结构

| 文件 | 说明 |
|------|------|
| `main/modules/iching/liuren_core.h` | 小六壬核心API |
| `main/modules/iching/liuren_core.c` | 农历转换+时辰获取+推算逻辑 |
| `main/ui/screens/liuren_screen.c/h` | 手掌指节UI + 老虎机动画 |

### 农历数据

内置 2024-2034 年农历数据表（春节日期、每月大小月、闰月信息）。`liuren_core.c` 中的 `solar_to_lunar()` 处理公历→农历转换。

## 番茄时钟（久坐提醒）

### 原理
利用 IMU 检测手柄横置姿态。左侧躺（`ay < -0.9`）启动 5 分钟休息计时，右侧躺（`ay > -0.9`）启动 30 分钟工作计时。时间到后界面显示"时间到!"。

侧躺时屏幕自动旋转保持内容竖直：左躺用 MADCTL MV+MX，右躺用 MADCTL MV+MY，通过 `esp_lcd_panel_swap_xy/mirror` 实现硬件旋转，无撕裂。

### 操作

| 操作 | 功能 |
|------|------|
| 左侧躺放置 1 秒 | 开始 5 分钟休息倒计时 |
| 右侧躺放置 1 秒 | 开始 30 分钟工作倒计时 |
| START+B | 退出模块 |
| START+A | 重置计时器，重新等待侧躺 |

### 倾斜检测逻辑
- 读取加速度计 `ay`
- 左躺：`ay < -0.9` → `esp_lcd_panel_swap_xy(true)` + `mirror(true, false)`
- 右躺：`ay > 0.9` → `esp_lcd_panel_swap_xy(true)` + `mirror(false, true)`
- 消抖：连续 1 秒相同姿态才触发（`side_debounce >= 1`）
- 旋转后调用 `st7789_clear()` 清 GRAM + `lv_obj_invalidate()` 触发 LVGL 重绘

### 位置补偿
- 竖直：`lv_obj_center()` 正中间
- 左躺/右躺：`lv_obj_align(LV_ALIGN_CENTER, 36, -28)`（向右 15%，向上 10%）

## IMU 校准模块

### 用途
当 IMU 轴映射不准确或需要更高精度时，通过 4 个标准姿势（竖握/左侧躺/右侧躺/平放）各采样 20 组数据，校准各轴的重力向量方向。

### 文件
| 文件 | 说明 |
|------|------|
| `main/modules/imu/imu_calib.h` | 校准 API (save/load/detect) |
| `main/modules/imu/imu_calib.c` | NVS 存储 + 向量角度匹配检测 |
| `main/ui/screens/calib_screen.h/c` | 校准 UI |

### 使用
菜单 → IMU → 按 A 开始每个姿势采样（各 2 秒）→ 自动保存到 NVS。之后 `imu_calib_detect()` 通过计算当前读数与校准向量的夹角确定姿势，阈值默认为 0.5（≈ 60°）。无校准时回退到固定阈值 0.7g。

## 屏幕旋转技术方案

采用 **ST7789 硬件 MADCTL 旋转**（非 LVGL 软件 `sw_rotate`）：
- 避免 `draw_buf_rotate` 分块刷新导致的撕裂
- 不占用额外 RAM（`sw_rotate` 需要 `LV_DISP_ROT_MAX_BUF=10KB` 临时缓冲）
- 通过 `esp_lcd_panel_swap_xy()` + `esp_lcd_panel_mirror()` 控制硬件寄存器
- 旋转后调整 `esp_lcd_panel_set_gap(0,0)` 避免 gap 偏移错误

### 关键文件
`main/ui/screens/countdown_screen.c` 中的 `set_hw_rot()` 函数。

## 调试指南
- 串口日志：`TIMER` 标签输出 `ax= ay= az=` 及 `ROT` 状态
- 重启常见原因：WiFi 驱动内存不足（`wifi:alloc eb fail`）→ 减少 LVGL 缓冲或增加 PSRAM
- 旋转残留：`st7789_clear()` 从 PSRAM 分配 134KB 全屏黑像素一次刷新

## Useful Scripts

| Script | Purpose |
|--------|---------|
| `_build.ps1` | Build (v5.4.1 path, older config) |
| `_flash.ps1` | Flash to COM3 |
| `_monitor.ps1` | 3-second serial read |
| `_verify.ps1` | Quick serial check |
| `_verify_weather.ps1` | Weather API verification |
| `_setup_python_env.ps1` | Python environment setup |
| `tools/pc_voice_receiver.py` | PC-side voice receiver |
| `tools/send_ip.py` | Send IP to device |

## Hardware Pin Map

```
LCD:    BL=IO9  DC=IO10 CS=IO11 SCK=IO12 MOSI=IO13 RST=IO14
SD:     CS=IO18 MISO=IO8   (shared SCK/MOSI with LCD)
Audio:  MCLK=IO40 BCLK=IO39 WS=IO45 DOUT=IO48 DIN=IO21
        Codec I2C: SDA=IO38 SCL=IO47
Sensor: I2C SDA=IO2 SCL=IO1
Keys:   UP=IO5 DOWN=IO6 LEFT=IO4 RIGHT=IO7 A=IO17 B=IO16 START=IO15
```

## 编码行为准则（Karpathy Guidelines）

源自 Andrej Karpathy 对 LLM 编码陷阱的观察。偏向谨慎，简单任务可灵活处理。

### 1. 先想后写

**不要假设，不要隐藏困惑，暴露权衡。**

- 明确陈述假设。不确定时提问。
- 存在多种理解时，全部列出——不要默默选一个。
- 有更简单的方案就说出来，该反驳时反驳。
- 有不清楚的地方就停下来，指出哪里困惑，请求澄清。

### 2. 简单优先

**解决问题的最小代码，不做推测性设计。**

- 不添加超出需求的功能。
- 不为一次性代码做抽象。
- 不加未被要求的"灵活性"或"可配置性"。
- 不为不可能的场景做错误处理。
- 200行能写成50行就重写。

检验标准：高级工程师会不会觉得过度设计？会就简化。

### 3. 精准改动

**只动必须动的，只清理自己制造的。**

编辑已有代码时：
- 不要"顺手改进"相邻代码、注释或格式。
- 不要重构没坏的东西。
- 匹配现有风格，即使你会写得不同。
- 发现无关的死代码就提一下，不要删。

改动产生孤儿代码时：
- 删除**你的改动**导致未使用的 import/变量/函数。
- 不删除已有的死代码，除非被要求。

检验标准：每一行改动都应该能直接追溯到用户的需求。

### 4. 目标驱动执行

**定义成功标准，循环直到验证通过。**

把任务转化为可验证的目标：
- "加校验" → "为无效输入写测试，然后让测试通过"
- "修bug" → "写一个能复现的测试，然后让测试通过"
- "重构X" → "确保重构前后测试都通过"

多步骤任务，列出简要计划：
```
1. [步骤] → 验证: [检查项]
2. [步骤] → 验证: [检查项]
3. [步骤] → 验证: [检查项]
```

明确的成功标准让Agent可以独立循环。模糊标准（"搞定就行"）会导致反复确认。
