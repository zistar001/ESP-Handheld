# ESP32-S3 Game固件开发进度

**更新日期:** 2026-06-08
**构建状态:** ✅ 编译通过
**测试状态:** ✅ NES游戏可玩（SD卡正常、进出游戏稳定）

---

## 概述

成功将Game固件（ESP_BSP）适配到gezipai板，解决了多个硬件兼容性问题，实现了双系统框架。NES模拟器基于esp32s3_nes_gamer项目的emucore核心，支持从TF卡加载ROM。

---

## 关键修复 (2026-06-08)

### 1. SD卡初始化顺序修复
- **问题:** SD卡初始化超时（ESP_ERR_TIMEOUT 0x107），无法加载ROM
- **根因:** 
  1. SD卡SPI时钟4MHz过高（初始化需要≤400kHz）
  2. 初始化顺序LCD先→SD后，SD加入时SPI总线已被LCD以60MHz配置
- **修复:**
  1. SD卡SPI时钟从4000kHz降到400kHz
  2. **初始化顺序: SD卡先（创建SPI总线）, LCD后加入**
  3. LCD的`st7789_init()`检测总线已存在时跳过spi_bus_initialize
- **引脚:** MISO=IO8, CS=IO18 (DAT3飞线)

### 2. 视频渲染: 逐行传输避免PSRAM DMA缓存问题
- **问题:** 全帧PSRAM缓冲(134KB)通过DMA发送时出现横条纹/花屏
- **根因:** PSRAM与SPI DMA之间的缓存一致性问题（需要`esp_cache_msync`）
- **修复:** 改为逐行渲染——栈变量`line_buf[240]`（内部SRAM）每行单独DMA传输
  ```c
  uint16_t line_buf[LINE_PIXELS];  // 栈上，内部SRAM — DMA安全
  st7789_spi_lock();
  for (int y = 0; y < 240; y++) {
      // 调色板转换 → line_buf
      esp_lcd_panel_draw_bitmap(panel, 0, voff+y, 240, voff+y+1, line_buf);
  }
  st7789_spi_unlock();
  ```

### 3. 游戏退出后卡死修复
- **问题:** 退出游戏后系统冻结，需重启
- **根因:** 
  1. **退出回调只用一次:** `s_exit_callback`在`video_task`中使用后设为NULL，第二次游戏退出时无回调→画面卡死
  2. **双重`app_manager_return`:** B+START退出时游戏任务先触发→on_game_exit调用app_manager_return；同时按键回调也可能触发，状态乱跳
  3. **加载中退出的竞态:** 用户选ROM后立即按START→`nes_game_stop()`设`s_running=false`→游戏任务加载完成后覆写`s_running=true`→SPI冲突
- **修复:**
  1. `nes_start()`每次调用重新注册回调: `nes_game_set_exit_callback(on_game_exit)`
  2. `on_game_exit`检查状态仅在`APP_STATE_RUNNING`时调用`app_manager_return()`
  3. 引入`s_abort`标志位: `nes_game_stop()`设置→游戏任务在启动循环前检查→中止

### 4. 帧率精确控制
- **问题:** 简化版`vTaskDelay`导致帧率不准
- **修复:** vTaskDelay粗粒度睡眠(10ms) + spin-wait补齐微秒级精度
  ```c
  const int32_t frame_us = 16667;  // 1000000/60
  if (remain > 11000) vTaskDelay(pdMS_TO_TICKS((remain - 10000) / 1000));
  while ((int32_t)(esp_timer_get_time() - t0) < frame_us) { asm volatile("nop"); }
  ```

### 5. 任务看门狗(WDT)修复
- **Core 1 (游戏任务):** 每帧调用`esp_task_wdt_reset()`喂狗
- **Core 0 (按键):** 将key_task优先级从5降至2，减少空闲任务饥饿

### 6. ST7789驱动: panel级gap配置
- **问题:** LVGL的`offset_y=20`和NES直接SPI绘制使用不同坐标系
- **修复:** 在`st7789_init()`中统一使用`esp_lcd_panel_set_gap(panel, 0, 20)`，LVGL的`offset_y`改为0

### 7. 死代码清理
- 删除冗余文件: `nes_port.cpp`, `nes_game_modified.c`, `CMakeLists.txt.bak`
- 统一NES模块: 只有`nes_game.c` + `nes_wrapper.c` + `components/nes_core/`

---

## 当前功能状态

### ✅ 正常工作
- LCD显示（240x280, ESP-IDF驱动, LV_COLOR_16_SWAP=1）
- AHT20温湿度传感器
- ES8311+ES7210音频编解码器
- WiFi管理器
- 天气和时间同步
- 应用管理器（菜单系统、ROM浏览器）
- 按键扫描（7键, 软件去抖）
- **SD卡（SPI模式, 400kHz, GPIO18 CS, 19个ROM识别）**
- **NES模拟器（60fps, 双核分离渲染/模拟, B+START退出）**
- **多次进出游戏稳定**

### ⚠️ 待修复
- 电池监控（ADC冲突，暂时禁用）
- IMU传感器（未检测到）
- NES颜色可能需调`esp_lcd_panel_invert_color`标志
- 不支持Mapper 83等部分mapper

---

## NES架构

```
Core 0                              Core 1
──────                              ──────
video_task (prio 1)                 game_task (prio 5)
  xTaskNotifyWait ←──────────────── xTaskNotify (每帧)
  Line-by-line SPI render            nes_renderframe(true)
  on_game_exit → LVGL restore        B+START → exit

key_task (prio 2)                  audio_timer (60Hz, FreeRTOS)
  GPIO扫描+去抖                      apu_process → I2S write
  → key_handler → nes_key_event
```

### 竞态保护标志位

| 标志 | 用途 |
|------|------|
| `s_running` | 游戏循环活跃? game_task设置, stop清除 |
| `s_abort` | 加载中被请求停止? `nes_game_stop()`设置, game_task检查后中止 |
| `s_game_done_sem` | 二进制信号量: game_task完成时give, `nes_game_stop()` take同步 |

---

## 内存分配

### NES模块
- SCREENMEMORY: 60KB (DMA) — NES显存
- cachedRom: 2MB (PSRAM) — ROM缓存

### 系统内存
- PSRAM: 8MB (Octal模式)
- Flash: 16MB
- 分区: factory(4MB) + ota_0(4MB) + ota_1(4MB) + assets(4MB)

---

## 编译和烧录

**重要:** 必须在cmd.exe或PowerShell中编译，MSYS2/Mingw bash不支持。

### Game固件
```bash
cd D:\code\claudecode\ESP_BSP
build.bat  # 编译+烧录
```

### 完整烧录命令（含otadata）
```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

---

## 关键教训

1. **PSRAM用Octal模式** (硬件要求)
2. **SPI mode 3** (gezipai板LCD要求)
3. **ST7789V需要颜色反转+Y偏移** (`set_gap(0,20)` + `invert_color(true)`)
4. **LV_COLOR_16_SWAP=1** (ST7789 SPI LCD必须字节交换)
5. **SD卡初始化顺序: SD先, LCD后** (共享SPI总线时)
6. **SD卡SPI初始化频率≤400kHz** (否则超时)
7. **逐行渲染用栈缓冲区** (避免PSRAM DMA缓存问题)
8. **退出回调每次重新注册** (回调只用一次)
9. **双重app_manager_return防护** (检查状态)
10. **游戏加载中退出需s_abort标志** (防止s_running被覆写)
11. **sdkconfig不能删除** (重建后缺失LVGL字体等关键配置)
