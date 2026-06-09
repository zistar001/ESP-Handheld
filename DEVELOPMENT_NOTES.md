# ESP_BSP — Development Notes

**Last Updated:** 2026-06-09
**Current Branch:** codex/nofrendo-migration
**Current Commit:** 2e39be7 (fix: home screen data persistence across navigation)
**Build Status:** ✅ Compiles and flashes
**Game Status:** ✅ NES playable, SD card working, exit/re-entry stable

---

## Project Overview

ESP32-S3 handheld device (ST7789 240x280 LCD + ES8311 audio + 7-key GPIO).
ESP-IDF v5.5.4 project, entry point `main/main.c`.
Platform: 16MB flash, 8MB PSRAM (Octal).

### Dual-System Partitions (custom partitions.csv)

| Partition | Offset   | Size   | Purpose          |
|-----------|----------|--------|------------------|
| nvs       | 0x9000   | 16K    | NVS              |
| otadata   | 0xD000   | 8K     | OTA data         |
| phy_init  | 0xF000   | 4K     | PHY init         |
| factory   | 0x10000  | 4MB    | ESP_BSP firmware |
| ota_0     | 0x410000 | 4MB    | OTA slot 0       |
| ota_1     | 0x810000 | 4MB    | OTA slot 1       |
| assets    | 0xC10000 | ~4MB   | SPIFFS assets    |

**Important:** Flash with `ota_data_initial.bin` at 0xd000 to ensure boot from factory.

---

## NES Game Module — Current Working Implementation

### Architecture

```
Main (Core 0)                     NES (Core 1)
───────────────                    ──────────────
app_manager                       game_task
  └─ rom_browser                    ├─ load_rom_file (SPI SD → PSRAM)
       └─ nes_start()               ├─ rom_getheader / rom_loadrom
            └─ nes_game_start()     ├─ nes_create / build_address_handlers
                 └─ xTaskCreate ──→ ├─ nes_renderframe(true) — 60fps loop
                                    └─ xTaskNotify(s_vid_task)

video_task (Core 0)               audio_timer (Core 0, FreeRTOS timer)
  ├─ xTaskNotifyWait               └─ apu_process → I2S write
  ├─ Line-by-line SPI transfer
  └─ on_game_exit → app_manager_return
```

### Key Design Decisions

1. **SD card first, LCD second**: SD initializes SPI bus at 400kHz (required for init), LCD joins later at 60MHz.
   - `sd_card_init()` creates SPI2_HOST bus
   - `st7789_init()` detects bus already exists, skips to panel IO

2. **Line-by-line video rendering**: Internal SRAM `line_buf[240]` avoids PSRAM DMA cache coherency issues.
   ```c
   uint16_t line_buf[LINE_PIXELS];  // stack, internal SRAM — DMA-safe
   st7789_spi_lock();
   for (int y = 0; y < 240; y++) {
       // convert palette → line_buf
       esp_lcd_panel_draw_bitmap(panel, 0, voff + y, 240, voff + y + 1, line_buf);
   }
   st7789_spi_unlock();
   ```

3. **Frame pacing**: vTaskDelay for coarse sleep (10ms granularity), spin-wait for µs precision:
   ```c
   const int32_t frame_us = 16667;  // 1000000/60
   if (remain > 11000) vTaskDelay(pdMS_TO_TICKS((remain - 10000) / 1000));
   while ((int32_t)(esp_timer_get_time() - t0) < frame_us) { asm volatile("nop"); }
   ```

4. **Task Watchdog**: `esp_task_wdt_reset()` called at each frame iteration — the spin-wait prevents idle task from running on Core 1.

### Exit Flow (Correct Pattern)

B+START held 3 frames → game_task sets `s_running=false` → cleanup → signals `s_game_done_sem` → notifies `s_vid_task` → deletes itself.

video_task wakes → calls `on_game_exit()`:
```c
static void on_game_exit(void) {
    ui_display_set_nes_active(false);   // restore LVGL rendering
    if (app_manager_get_state() == APP_STATE_RUNNING) {
        app_manager_return();            // MENU transition
    }
}
```

**Critical:** `nes_game_set_exit_callback(on_game_exit)` must be called in `nes_start()` (not just `nes_wrapper_init()`) because `s_exit_callback` is set to NULL after each use in `video_task`.

### Race Condition Prevention

| Flag | Purpose |
|------|---------|
| `s_running` | Game loop active? Set by game_task, cleared by stop |
| `s_abort` | Stop requested during ROM loading? Set by `nes_game_stop()`, checked by game_task before entering loop |
| `s_game_done_sem` | Binary semaphore: game_task gives on cleanup, `nes_game_stop()` takes to synchronize |

Stop during ROM loading flow:
1. `nes_game_stop()` → sets `s_abort=true`, `s_running=false` → waits on `s_game_done_sem`
2. game_task finishes loading → checks `s_abort` → `goto done` → gives semaphore → deletes
3. `nes_game_stop()` gets semaphore → clears `s_abort` → returns clean

### Hardware Pin Configuration

```
LCD:    BL=IO9  DC=IO10 CS=IO11 SCK=IO12 MOSI=IO13 RST=IO14
SD:     CS=IO18 (飞线) MISO=IO8   (shared SCK/MOSI with LCD)
Audio:  MCLK=IO40 BCLK=IO39 WS=IO45 DOUT=IO48 DIN=IO21
        Codec I2C: SDA=IO38 SCL=IO47
Sensor: I2C SDA=IO2 SCL=IO1
Keys:   UP=IO5 DOWN=IO6 LEFT=IO4 RIGHT=IO7 A=IO17 B=IO16 START=IO15
```

---

## Build & Flash

### Prerequisites
- ESP-IDF v5.5.4, target esp32s3
- Python: `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe`
- Serial: COM7 (115200 monitor / 921600 flash)
- **IMPORTANT:** Build must run from cmd.exe or PowerShell, NOT MSYS2/Mingw bash
  (ESP-IDF detects MSYSTEM env var and refuses to run)

### PowerShell Build Script Pattern
```powershell
$env:MSYSTEM=''  # Clear MSYS2 detection
$env:IDF_PATH='D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH='D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='0'
$env:PATH='...\cmake\bin;...\ninja;...\xtensa-esp-elf\bin;...\python\Scripts;' + $env:PATH
python D:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py build
```

### Flash Command (full, including otadata)
```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

### sdkconfig
- DO NOT delete `build/` directory without backing up `sdkconfig`
- The `sdkconfig` at project root contains critical settings (LVGL fonts, partition scheme, PSRAM mode)
- `idf.py set-target esp32s3` regenerates from `sdkconfig.defaults` which is incomplete

### Component Manager
- **Must be enabled** (`IDF_COMPONENT_MANAGER=1`) for zlib dependency (`espressif/zlib: ^1.3.1`)
- The `managed_components/` directory is auto-generated — do not commit
- `dependencies.lock` should be committed

---

## Known Issues

1. **Color may be incorrect**: `esp_lcd_panel_invert_color(panel, true)` in `st7789_init()`. If colors look inverted, try `false`.
2. **Unsupported NES mappers**: Only mappers 0,1,2,3,4,7,11,15, etc. are supported. ROMs with mapper 83 (Fatal Fury 2) etc. will fail with "Unsupported mapper".
3. **Task WDT on CPU 1 idle**: Game task at 60fps with spin-wait starves IDLE1. Non-fatal with `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n`. WDT warning appears in serial but game continues.
4. **SPI actual speed**: `pclk_hz=60MHz` results in ~40MHz actual on ESP32-S3 (APB=80MHz, minimum SPI clock divider=2). The NES frame transfer takes ~26ms, limiting video refresh to ~38fps. Game logic stays at 60fps via fire-and-forget.

## Git Branches

| Branch | HEAD | Description |
|--------|------|-------------|
| master | e9ed773 | Original NES module (incomplete CMakeLists.txt) |
| codex/nofrendo-migration | current | Current: full UI + working NES + weather |

---

## Resolved Issues

### NES Game Running at 27fps (Slow Motion)

**Root cause:** The `game_task` had a `xTaskNotifyWait()` that waited for the `video_task` to finish SPI transfer before starting the next frame. Since SPI transfer at 40MHz takes ~26ms and emulation takes ~10ms, total frame time was ~36ms → 27fps.

**Fix:** Changed to fire-and-forget pattern — `game_task` notifies `video_task` and immediately continues to the next frame without waiting. The game logic runs at 60fps (frame-paced), while the display updates at whatever rate the SPI can manage (~38fps). No visual tearing because of the PSRAM shadow buffer.

**Key changes in `nes_game.c`:**
1. Removed `xTaskNotifyWait()` from game loop — game no longer waits for SPI
2. Added PSRAM shadow buffer (`s_shadow`) — `video_task` reads from shadow while `game_task` writes to `SCREENMEMORY`, preventing tearing
3. Added `esp_task_wdt_add(NULL)` — fixed WDT subscription for IDF v5.x (was causing continuous WDT error spam)
4. Kept `frame_us = 16667` frame pacing — ensures game logic runs at exactly 60fps via spin-wait

### Screen Black Horizontal Stripes in LVGL UI

**Root cause:** `disp_flush_cb` called `lv_disp_flush_ready(drv)` before the SPI DMA transfer completed. LVGL reused the single buffer while DMA read corrupted data → stripes.

**Fix:** Changed LVGL display buffer from single to double-buffer (`display_driver.c`). DMA sends buffer A while LVGL renders to buffer B.

### LVGL Freeze/Crash After Multiple Screen Transitions

**Root cause:** Every screen transition (`home_screen_create`, `menu_screen_create`, `rom_browser_enter`, etc.) created a new LVGL screen with `lv_obj_create(NULL)` and `lv_scr_load(new)`, but **never deleted the old screen**. Old screens accumulated, exhausting LVGL's 64KB heap (`LV_MEM_SIZE`). After ~3-5 transitions, memory allocation failed → freeze.

Additionally, all LVGL operations were called from the `key_driver_scan_task` (priority 2) while `lv_timer_handler()` ran in `lvgl_tick_task` (priority 1). LVGL is NOT thread-safe — concurrent access from different tasks corrupts internal state.

**Fixes:**

1. **Screen leak fix** — Every screen create function now saves `lv_scr_act()` before creating a new screen, and calls `lv_obj_del(old)` after `lv_scr_load(scr)`:
   - `home_screen_create()` / `menu_screen_create()` / `countdown_screen_create()` / `settings_screen_create()` / `rom_browser_enter()`
   - WiFi setup & About screens in `app_manager_launch()`

2. **LVGL mutex** — Added `lvgl_lock()` / `lvgl_unlock()` with a FreeRTOS mutex (`display_driver.c`):
   - `lvgl_tick_task` locks before `lv_timer_handler()`
   - `key_handler` in `main.c` locks around all LVGL operations
   - `rom_browser_key()` locks around navigation

**Key lesson:** Every `lv_obj_create(NULL)` needs a corresponding `lv_obj_del(old_scr)` when switching screens. LVGL is not thread-safe — all LVGL calls must happen from or be serialized through one thread.

### System Event Task Stack Overflow (Reboot Loop)

**Root cause:** `sys_evt` task had only 2304 bytes stack — overflowed during WiFi event processing.

**Fix:** `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096` in sdkconfig.

### Weather Module Gzip — Server Always Returns Compressed Data

**Root Cause:** The HeFeng weather API (`p23p3qvugk.re.qweatherapi.com` and `devapi.qweather.com`) ignores the `&gzip=n` URL parameter and always returns gzip-compressed JSON. The `esp_http_client` event handler collects raw bytes, which are gzip'd — `cJSON_Parse()` fails on compressed data.

**Previous failed attempts:**
1. ROM `miniz` `tinfl_decompress_mem_to_mem` → crashes on ESP32-S3 (unsafe with PSRAM config)
2. Custom inflate implementation → various bugs (Huffman table fill, bit ordering)
3. `esp_http_client_read()` after `perform()` → returns 0 (body already consumed by event handler)
4. `esp_http_client_open()` + `fetch_headers()` + `read()` → transport layer doesn't auto-decompress

**Final Fix:** Use the standard `zlib` library via ESP-IDF component manager:
1. Add `espressif/zlib: "^1.3.1"` to `main/idf_component.yml`
2. Add `zlib` to `REQUIRES` in `main/CMakeLists.txt`
3. Enable `IDF_COMPONENT_MANAGER=1` (was 0, disabled)
4. Use `inflateInit2(&strm, 16 + MAX_WBITS)` + `inflate()` with **full gzip data** (including header)
5. zlib auto-detects gzip format with windowBits=31

**Key lesson:** zlib's `inflateInit2` with `16 + MAX_WBITS` (31) auto-detects gzip vs zlib format. Pass the complete data stream including the gzip header — don't strip it.

### PSRAM + mbedTLS SSL Memory (Device Crashed After Weather Fetch)

**Root Cause:** `CONFIG_SPIRAM_USE_MALLOC=y` caused `malloc()` to allocate from PSRAM by default. Components that need DMA-accessible memory (mbedTLS hardware crypto, I2S, WiFi) received PSRAM addresses → crash when hardware attempted DMA.

**Fix:** Reverted to `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (original config). The `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` (already set) handles SSL certificate allocation to PSRAM via `heap_caps_malloc()`. Added `CONFIG_MBEDTLS_DYNAMIC_BUFFER=y` and `CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE=n` for memory optimization.

### NES SCREENMEMORY in Internal SRAM (61KB Waste)

**Root Cause:** `SCREENMEMORY` (256×240 = 61KB) was allocated with `MALLOC_CAP_DMA` forcing it into internal SRAM. The actual data flow doesn't need DMA for this buffer — it's CPU-rendered then `memcpy()`'d to a PSRAM shadow buffer.

**Fix:** Changed to `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, freeing 61KB of internal SRAM for WiFi, audio DMA, and mbedTLS.

### Home Screen Data Lost on Navigation Return

**Root Cause:** `home_screen_create()` always deleted the old screen (`lv_obj_del(lv_scr_act())`) and created a brand new screen with all labels initialized to `"--:--"` / `"---"`. When the user entered the menu (which also deletes the home screen), then returned, all data (time, weather, indoor data) was gone. The `launcher_update_task` eventually refilled the time/indoor data, but weather required a new HTTP request cycle (30 min interval).

**Fix:** Added a data cache layer in `home_screen.c`:
- Static variables cache time, date, weather, forecast, indoor temp/hum
- `home_screen_update_*()` functions save to cache AND update labels
- `home_screen_create()` restores from cache after screen recreation
- `s_cached_data_valid` flag ensures data is only restored after at least one fetch

**Key lesson:** When switching between LVGL screens, the old screen object is deleted by the new screen's creator. To preserve data, explicitly cache it in static variables and restore after recreation.

### Important Lessons Learned

**1. LVGL单缓冲+DMA异步传输=条纹**
- `lv_disp_flush_ready` 在DMA完成前调用，LVGL会修改正在被DMA读取的缓冲区
- 修复：双缓冲，DMA读A时LVGL写B

**2. game_task不能等video_task**
- SPI传输40MHz下要26ms，游戏模拟只要10ms
- 等待=把游戏拖慢到显示速度（27fps）
- 修复：fire-and-forget，游戏维持60fps，显示能跑多少跑多少

**3. PSRAM影子缓冲替代等待**
- fire-and-forget时game_task写SCREENMEMORY，video_task读s_shadow
- 每帧模拟完后memcpy到shadow（0.5ms），保证视频读到稳定数据

**4. 不要盲目降低SPI时钟**
- 60MHz→40MHz让帧传输从15ms变26ms，雪上加霜
- ESP32-S3实际SPI最大只有40MHz（APB=80MHz，pre_div最小2）

**5. 不要覆盖ESP-IDF已设寄存器**
- 0xB0 RAMCTRL覆盖破坏了大端模式，颜色全乱

**6. GCC 14+兼容**
- `size_t`要用`%zu`，C++需显式`#include <cstring>`
- `file(TO_CMAKE_PATH)`需给空变量加引号

---

## Git Branches

| Branch | HEAD | Description |
|--------|------|-------------|
| master | e9ed773 | Original NES module (incomplete CMakeLists.txt) |
| codex/nofrendo-migration | 723029e | Current: full UI + working game module |

### Commit History
```
2e39be7 — Home screen data persistence (cache + restore)
2f58353 — Weather gzip via zlib, memory optimizations
5a073cb — Ignore .claude/ and managed_components/
46b1f5e — NES refactor (SD card, rendering, exit freeze, WDT)
723029e — DFS disable, LVGL double-buffer, WiFi, remove BLE
e9ed773 — NES game module (gamer project emucore)
6e04d4a — sensor drivers + SD card SPI
a931d41 — GPIO key driver
0f95b8c — BSP + ST7789 driver
cb98dc7 — initial scaffold
```
