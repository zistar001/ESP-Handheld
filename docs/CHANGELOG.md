# ESP32-S3 Handheld Game - 变更记录

## 2026-06-08 — NES游戏系统重构

### 一、SD卡修复
- **问题:** SPI时钟4MHz过高，初始化超时
- **修复:** 降至400kHz（SD规范要求）；初始化顺序改为SD先→LCD后

### 二、视频渲染修复
- **问题:** PSRAM全帧缓冲DMA导致横条纹/花屏
- **修复:** 逐行渲染，栈变量`line_buf[240]`(内部SRAM)，每行独立DMA传输

### 三、游戏退出后卡死修复（关键）
- **问题1:** 退出回调只用一次 → 第二次退出时`s_exit_callback=NULL` → 画面卡死
  - **修复:** `nes_start()`每次调用重新注册`on_game_exit`
- **问题2:** 双重`app_manager_return()` → 状态乱跳
  - **修复:** `on_game_exit`检查状态，仅在`APP_STATE_RUNNING`时转换
- **问题3:** 加载中退出竞态 → `s_running`被游戏任务覆写 → SPI冲突
  - **修复:** 引入`s_abort`标志位，游戏任务在启动循环前检查

### 四、帧率精确控制
- vTaskDelay粗粒度(10ms) + spin-wait微秒补齐，保证稳定60fps

### 五、任务看门狗
- Core 1: 每帧`esp_task_wdt_reset()`喂狗
- Core 0: key_task优先级5→2

### 六、ST7789 panel级gap
- `esp_lcd_panel_set_gap(panel, 0, 20)`统一LVGL和NES坐标系
- LVGL的`offset_y`从20改为0

### 七、死代码清理
- 删除: `nes_port.cpp`, `nes_game_modified.c`, `CMakeLists.txt.bak`
- 统一实现: `nes_game.c` + `nes_wrapper.c`

---

## 2026-06-07 — 性能优化 (14fps → 目标60fps)

### 1. 内存分配优化 (components/nes_core/emucore.cpp)
- 模拟器核心结构体从 MALLOC_CAP_SPIRAM 改为 MALLOC_CAP_DMA：ppu_t, apu_t, mmc_t, nes_t, cpu, CPU RAM, SRAM, VRAM
- NesStateBlocks 保留在 SPIRAM (仅存档用)

### 2. 任务核心分配 (main/modules/nes/nes_game.c)
- video_task → Core 0 (LCD渲染)
- game_task → Core 1 (模拟器)
- 参考 esp32s3_nes_gamer 架构: 双核并行

### 3. 帧同步优化
- 移除同步阻塞，改为异步通知 (xTaskNotify)
- video_task 优先级从5降至1

### 4. 音频: 定时器驱动
- 独立 FreeRTOS 定时器(60Hz)，直接 i2s_channel_write()
- 绕过 codec 层降低延迟

### 5. BLE 共存优化
- 启动顺序: NES→BLE→WiFi (NES先分配DMA)

### 6. 系统稳定性
- 任务看门狗: 禁用IDLE检查 (模拟器占用CPU)

---

## 参考项目
- 性能架构: D:\code\claudecode\esp32s3_nes_gamer
- 驱动/WiFi: D:\code\claudecode\ESP_BSP\xiaozhi-esp32-main
- NES核心: D:\code\claudecode\Rachel (nofrendo参考)

## 已知问题
- WiFi init 可能因DMA不足失败 (BLE+NES占用DMA)
- 部分NES mapper不支持 (mapper 83等)
- NES颜色可能需要调`esp_lcd_panel_invert_color`标志
