# ESP32-S3 Handheld Game - 变更记录
日期: 2026-06-07

## 一、性能优化 (14fps → 目标60fps)

### 1. 内存分配优化 (components/nes_core/emucore.cpp)
- 模拟器核心结构体从 MALLOC_CAP_SPIRAM 改为 MALLOC_CAP_DMA：ppu_t, apu_t, mmc_t, nes_t, cpu, CPU RAM, SRAM, VRAM
- NesStateBlocks 保留在 SPIRAM (仅存档用)
- 受影响行: 68,84,120,1541,1543,3251,3253,3579,3627,3629,3638,3640,3649

### 2. 任务核心分配 (main/modules/nes/nes_game.c)
- video_task → Core 0 (LCD渲染)
- game_task → Core 1 (模拟器) ← 修复:原为Core0
- 参考 esp32s3_nes_gamer 架构:双核并行运行

### 3. 帧同步优化 (main/modules/nes/nes_game.c)
- 移除 game_task 等待 video_task 的同步阻塞
- 改为异步通知 (xTaskNotify)，恢复正确帧率计时
- video_task 优先级从5降至1

### 4. 音频:定时器驱动 (main/modules/nes/nes_game.c)
- 旧:在 video_task 中通过 box_audio_write() 推送 (经过codec层)
- 新:独立 FreeRTOS 定时器(60Hz)，直接 i2s_channel_write()
- 绕过 codec 层降低延迟和抖动
- 参考 esp32s3_nes_gamer 的 xTimerCreate + i2s_write 方案

### 5. SCREENMEMORY→SPIRAM (main/modules/nes/nes_game.c)
- 从 MALLOC_CAP_DMA 改为 MALLOC_CAP_SPIRAM
- SPI DMA 使用栈变量 line_buf, SCREENMEMORY 无需DMA
- 节省 60KB DMA 供 BLE 使用

## 二、BLE 共存优化

### 6. 启动顺序 (main/main.c)
- 旧: BLE→WiFi→NES  ← NES因DMA不足分配失败
- 新: NES→BLE→WiFi  ← NES先分配DMA, BLE随后

### 7. BLE HID 保护 (main/modules/pc_remote/ble_hid.c)
- 添加 s_initialized 守卫使 ble_hid_init() 可重入

### 8. 游戏退出流程 (main/modules/nes/nes_wrapper.c)
- on_game_exit(): 加200ms延迟让SPI DMA静默后再重建BLE
- nes_stop(): 移除冗余 ble_hid_init()

## 三、系统稳定性

### 9. 任务看门狗 (sdkconfig.defaults)
- CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n
- CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n
- 模拟器占用CPU时不会被WDT重置

### 10. WiFi/DMA 优化 (参考 xiaozhi-esp32-main)
- CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=3
- CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=6
- CONFIG_ESP_WIFI_RX_BA_WIN=3
- CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y
- CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA=y
- CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=16

## 参考项目
- 性能架构: D:\code\codex\esp32s3_nes_gamer
- 驱动/WiFi: D:\code\codex\ESP_BSP\xiaozhi-esp32-main

## 已知问题
- WiFi init 可能因DMA不足失败 (BLE+NES占用太多DMA)
- 游戏退出后 BLE 重建时 DMA 分配可能暂时失败

