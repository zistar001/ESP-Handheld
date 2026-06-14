# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Build & Flash

**IMPORTANT: Never build from MSYS2/Mingw bash.** ESP-IDF detects `MSYSTEM` and refuses. Use cmd.exe or PowerShell.

## One-Click Build+Flash

Run `_bf.ps1` from PowerShell — builds with IDF v5.5.4, flashes full image to COM7.

## Canonical Build (PowerShell)

```powershell
# Set environment (adjust paths to match your IDF install)
$env:MSYSTEM=''
$env:IDF_PATH='D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH='D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='0'   # set to '1' only when zlib/weather is needed
# Add cmake, ninja, toolchain, python to PATH
python $env:IDF_PATH\tools\idf.py build
```

**Note:** `IDF_COMPONENT_MANAGER=1` is required for the weather module (zlib). Most builds don't need it — keep it at `0` for faster offline builds. See "Component Manager" in Critical Gotchas.

## Flash (must include ota_data_initial.bin)

```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

**Without 0xd000 (ota_data_initial.bin), the device boots into XiaoZhi AI (ota_1) instead of factory.**

## Flash app-only (quick iteration)

```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 write_flash 0x10000 build/esp_handheld.bin
```

## Flash SPIFFS assets partition

```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 write_flash 0xC10000 build/assets.bin
```

Build the SPIFFS image: `python $env:IDF_PATH/components/spiffs/spiffsgen.py 0x3F0000 assets/ build/assets.bin`

## Monitor

```bash
idf.py -p COM7 monitor   # 115200 baud, Ctrl+] to exit
```

## Flash XiaoZhi AI to ota_1 (dual-system)

```bash
esptool.py -p COM7 -b 921600 write_flash 0x810000 build/xiaozhi.bin
```

XiaoZhi source is at `xiaozhi-esp32-main/`, must be compiled with the same `partitions.csv` (position-dependent, app linked at 0x810000).

## Quick scripts

| Script | Purpose |
|--------|---------|
| `_bf.ps1` | **Primary** — build+flash (IDF v5.5.4, COM7) |
| `_build.ps1` | Build only (IDF v5.4.1, COMPONENT_MANAGER=1 for zlib) |
| `_build_v5_4.ps1` | Build only (IDF v5.4.1, no component manager) |
| `_flash.ps1` | Full flash to COM3 (legacy, v5.4.1) |
| `_flash_app.ps1` | App-only flash to COM3 (legacy) |
| `_monitor.ps1` | 3-second serial read on COM3 |
| `_verify.ps1` | Quick serial check on COM3 |
| `_verify_weather.ps1` | Weather API verification |
| `_setup_python_env.ps1` | Python environment setup |
| `tools/pc_voice_receiver.py` | PC-side voice receiver (WiFi audio) |
| `tools/send_ip.py` | Send IP to device |

**Note:** There are TWO IDF versions in use — the project migrated from v5.4.1→v5.5.4. Legacy scripts target COM3/v5.4.1; primary scripts (`_bf.ps1`) target COM7/v5.5.4. Use `_bf.ps1` for new work.

# Environment

| Variable | Value |
|----------|-------|
| IDF_PATH | `D:\Espressif\frameworks\esp-idf-v5.5.4` |
| IDF_TOOLS_PATH | `D:\Espressif\tools` |
| Target | `esp32s3` |
| Serial port | COM7 |
| Flash baud | 921600 |
| Python venv | `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe` |

# Project Overview

ESP32-S3 handheld device firmware — NES emulator + LVGL UI + weather + BLE remote + WiFi audio + dual-firmware with XiaoZhi AI. Built with **ESP-IDF v5.5.4**, project name `esp_handheld`. Entry point: `main/main.c`.

**Hardware:** ST7789 240×280 SPI LCD, ES8311 audio, ES7210 mic, LSM6DS3TR-C IMU, AHT20 temp/humidity, 7 physical keys, 16MB flash, 8MB Octal PSRAM.

## Dual-Firmware Partitions

| Partition | Offset | Size | Content |
|-----------|--------|------|---------|
| factory | 0x10000 | 4MB | ESP_BSP (Game firmware) |
| ota_0 | 0x410000 | 4MB | Game OTA slot |
| ota_1 | 0x810000 | 4MB | XiaoZhi AI firmware |
| assets | 0xC10000 | ~4MB | SPIFFS resources |

System switching via `esp_ota_set_boot_partition()` + `esp_restart()`. In menu, selecting AI toggles: if running from factory/ota_0 → boot ota_1, if running from ota_1 → boot factory.

# Code Architecture

## Module Layout

```
main/
  main.c                — Entry: init modules, create tasks, key handler
  bsp/                  — Board support (st7789, sd_card, bsp_board, key_driver)
  app/                  — App framework (app_manager, launcher, menu, rom_browser)
  modules/
    nes/                — NES emulator (nes_game, nes_wrapper, font8x16)
    audio/              — ES8311 DAC + ES7210 ADC + box_audio_codec
    imu/                — LSM6DS3TR-C driver + calibration
    sensor/             — AHT20 temp/humidity
    weather/            — HeFeng weather API (HTTPS + zlib gunzip)
    pc_remote/          — BLE HID keyboard/mouse, air mouse, WiFi audio
    wifi_manager/       — WiFi connect/provisioning (esp-wifi-connect bridge)
    power/              — Battery ADC monitor
    settings/           — NVS settings (9 fields)
    time_sync/          — SNTP + NVS backup
    xiaozhi/            — Dual-firmware switch wrapper
    iching/             — I Ching + Xiao Liu Ren fortune divination
  ui/
    display_driver.c    — LVGL init, double-buffer, mutex
    screens/            — home, menu, settings, countdown, airmouse, kbd,
                          ip_input, fortune, liuren, calib, record
    components/         — status_bar, weather_icons
components/
  lvgl/                 — LVGL v8.4 (local, not from registry)
  lv_conf.h             — LVGL config
  nes_core/             — NES emulator core (C++, from esp32s3_nes_gamer)
  esp-wifi-connect/     — WiFi provisioning (github.com/78/esp-wifi-connect)
  espressif__esp_codec_dev/ — Codec device abstraction
```

## FreeRTOS Task Layout

| Task | Stack | Prio | Core | Role |
|------|-------|------|------|------|
| key | 2048 | 5 | 0 | Key scan (software debounce, 3-sample / 10ms) |
| sensor | 3072 | 3 | 0 | AHT20 + home screen update |
| imu | 3072 | 3 | 0 | IMU read + attitude |
| airmouse | 3072 | 3 | 0 | Air mouse calcs (gyro→BLE mouse delta) |
| batt | 3072 | 2 | 0 | Battery monitoring (10s interval) |
| pm | 2048 | 1 | 0 | Power mgmt, Light Sleep check (5s) |
| game_task | — | — | 1 | NES emulation (60fps game loop) |
| video_task | — | — | 0 | NES frame SPI transfer (~38fps) |

## App State Machine (app_manager)

```
Launcher (home screen) ──[START]──→ Menu (2-column grid)
Menu ──[A select]──→ Running (app: NES/settings/BLE/etc.)
Running ──[B/START]──→ Menu
Menu ──[B/START]──→ Launcher
```

**Menu screen:**
- 2-column grid, 10 app cards with icon + label. Selection highlight = 3px golden border (`0xFFBB00`) + 7% fill tint + glow shadow.
- Navigation stops at grid edges (no column wrapping).
- Returning from an app remembers the last selected card position.

## NES Emulator Architecture

NES runs as two communicating tasks on separate cores:

```
game_task (Core 1)                           video_task (Core 0)
  └─ load_rom_file (SPI SD → PSRAM)          └─ xTaskNotifyWait
  └─ nes_create / build_address_handlers      └─ Line-by-line SPI transfer
  └─ nes_renderframe(true) — 60fps loop       └─ on_game_exit → app_manager_return
  └─ xTaskNotify(s_vid_task) ──────────────→
```

**Fire-and-forget:** game_task notifies video_task and immediately starts the next frame without waiting for SPI. Game logic stays at 60fps; display runs at ~38fps (SPI bottleneck). **PSRAM shadow buffer** (`s_shadow`) prevents tearing: game_task writes SCREENMEMORY, video_task reads the shadow.

**Frame pacing:** Coarse sleep via `vTaskDelay` (10ms granularity), µs-precision spin-wait:
```c
const int32_t frame_us = 16667;  // 1000000/60
if (remain > 11000) vTaskDelay(pdMS_TO_TICKS((remain - 10000) / 1000));
while ((int32_t)(esp_timer_get_time() - t0) < frame_us) { asm volatile("nop"); }
```

**Exit flow (B+START held 3 frames):**
1. game_task sets `s_running=false` → cleanup → signals `s_game_done_sem` → notifies `s_vid_task` → deletes itself
2. video_task wakes → calls `on_game_exit()` → restores LVGL rendering, calls `app_manager_return()`
3. **Critical:** `nes_game_set_exit_callback()` must be called in `nes_start()` (not just `nes_wrapper_init()`) — `s_exit_callback` is set to NULL after each use

**Race condition prevention:**

| Flag | Purpose |
|------|---------|
| `s_running` | Game loop active? Set by game_task, cleared by stop |
| `s_abort` | Stop requested during ROM loading? Set by `nes_game_stop()`, checked by game_task before entering loop |
| `s_game_done_sem` | Binary semaphore: game_task gives on cleanup, `nes_game_stop()` takes to synchronize |

Supported mappers: 0,1,2,3,4,7,11,15 etc. (from esp32s3_nes_gamer's nes_core).

## LVGL Display Pipeline

- **Double buffer** in PSRAM (DMA sends buffer A while LVGL renders buffer B). Single buffer causes horizontal stripes.
- **NOT thread-safe** — all LVGL calls must go through `lvgl_lock()` / `lvgl_unlock()` (defined in `display_driver.c`)
- Every `lv_obj_create(NULL)` must pair with `lv_obj_del(old_scr)` when switching screens — old screens leak and exhaust LVGL's 64KB heap
- NES active → LVGL timer_handler paused (`ui_display_set_nes_active()`)

## Audio Subsystem

I2S_NUM_0 shared bus: TX (standard mode) → ES8311 DAC → speaker, RX (TDM 4-slot) → ES7210 ADC → dual mic. 16kHz, 16bit. Volume/mic gain NVS-persisted.

## BLE HID

Bluedroid BLE, HID keyboard + mouse. Just Works pairing, device name "ESP-Handheld". Passkey hardcoded "1234". API: `ble_hid_send_key(modifier, key)`, `ble_hid_send_mouse(buttons, dx, dy)`.

## WiFi Manager

Uses `78/esp-wifi-connect` v3.1.4 (C++, bridged to C via `wifi_bridge.cpp`). Soft-AP + DNS hijack + built-in web config page. Multi-SSID storage via SsidManager. Auto-migrates old settings_manager WiFi data.

# I Ching Fortune (运势)

Menu → 运势 → 6 selections (general/ career/ business/ fame/ love/ decision) → shake device (IMU) to generate hexagram lines.

## Screen Layout

**Phase 0 (category select):** 6 buttons (30px height, step 36px) fit without scrolling. Title "选择占卜事项" centered.

**Phase 1 (shaking):** Category name + "心念专一，摇动起卦" centered at top. Yao lines grow from bottom up, evenly distributed across the vertical space (~y=77 to y=252, leaving 10% bottom margin). Each line is 12px tall with 23px gap.

**Phase 2 (result):** "心念专一，摇动起卦" replaced by centered "第X卦 卦名". Compact hexagram (60px wide ≈ 25% screen, 4px lines, 3px gaps) at y=120 upward. Below: scrollable judgment text left-aligned. Navigation UP/DOWN scrolls the text.

## Shake Detection

- Threshold: Y-axis deviates from 1g by >0.6g, rest below 0.2g
- 3 strong shakes = 1 yao (random bits from `esp_random()`)
- Cooldown: 600ms between generated lines

## Critical: Hexagram ID Mapping

`g_iching[]` is in **King Wen order** (1=乾, 2=坤, …, 64=未济), NOT binary order. The `get_hexagram_id()` function computes a binary index `(upper<<3)|lower` using Fu Xi trigram numbering (top line = bit 0, bottom line = bit 2). The `binary_to_index[64]` table translates binary→King Wen array index.

```
lower = s_yao[2] | (s_yao[1]<<1) | (s_yao[0]<<2);  // top=bit0, bottom=bit2
upper = s_yao[5] | (s_yao[4]<<1) | (s_yao[3]<<2);
```

**Files:**
- `modules/iching/iching_data.c/h` — 64 hexagrams data + `binary_to_index[]` mapping
- `ui/screens/fortune_screen.c/h` — Selection → shake → result UI

**Regenerate hexagram data:**
```bash
python gen_iching.py   # from YI64.md → iching_data.c
```

**If regenerating, also rebuild binary_to_index[]** — see the mapping formula above.

# Xiao Liu Ren (小六壬)

Menu → 小六壬 → palm interface → press **A** to trigger. 6 spirit positions on finger joints light up in sequence (white → yellow), cycling 3 rounds, stopping at the result.

**Screen layout:**
- Title "小六壬" at top. White segments form a palm shape with spirit names (大安/留连/速喜/赤口/小吉/空亡) centered vertically within each segment. Finger names (大拇/食指/中指/无名/小) alongside.
- Below the palm: 3 centered explanation lines + "掐指一算" button.
- After animation: 0.5s delay, then palm is replaced by a compact view: only the result spirit name centered at top, with full scrollable judgment text below (lunar date, hexagram name, meaning, interpretation).

**Animation:** Segments light in cycle order (大安→留连→速喜→赤口→小吉→空亡), 3 rounds at 60ms, final stop at 80ms per position. Final result highlighted.

**Files:**
- `modules/iching/liuren_core.c/h` — Lunar calendar conversion + time acquisition + divination logic
- `ui/screens/liuren_screen.c/h` — Palm finger UI + slot-machine animation

Lunar data table built-in for 2024–2034 (Spring Festival dates, month lengths, leap months).

**Static pointer safety:** All object pointers (segments, labels, scroll container) must be reset on `liuren_screen_create()` because the old screen is deleted on exit, leaving dangling pointers. Missing a reset causes a crash on the second invocation.

# Countdown / Pomodoro (番茄时钟)

IMU tilt detection: left tilt (`ay < -0.9`) starts 5-min rest timer, right tilt (`ay > 0.9`) starts 30-min work timer. Screen auto-rotates via ST7789 hardware MADCTL (not LVGL sw_rotate) to stay upright.

| Action | Result |
|--------|--------|
| Left tilt >1s | 5-min rest countdown |
| Right tilt >1s | 30-min work countdown |
| START+B | Exit module |
| START+A | Reset, wait for tilt |

**Files:**
- `ui/screens/countdown_screen.c/h`

# IMU Calibration

Calibrates IMU axis mapping via 4 standard poses (vertical hold / left tilt / right tilt / flat), 20 samples each. Saves to NVS. After calibration, `imu_calib_detect()` uses vector-angle matching to determine pose (default threshold 0.5 ≈ 60°). Falls back to fixed 0.7g threshold when uncalibrated.

**Files:**
- `modules/imu/imu_calib.c/h` — Calibration API (save/load/detect)
- `ui/screens/calib_screen.c/h` — Calibration UI

# Screen Rotation (ST7789 Hardware MADCTL)

Uses hardware MADCTL registers (NOT LVGL software `sw_rotate`):
- Avoids tearing (`sw_rotate` needs `LV_DISP_ROT_MAX_BUF=10KB` temp buffer)
- No extra RAM consumed
- Uses `esp_lcd_panel_swap_xy()` + `esp_lcd_panel_mirror()` to control registers
- Resets gap via `esp_lcd_panel_set_gap(0,0)` after rotation

Key function: `set_hw_rot()` in `ui/screens/countdown_screen.c`.

Position compensation: vertical = `lv_obj_center()`, tilted = `lv_obj_align(LV_ALIGN_CENTER, 36, -28)`.

# Debugging Guide

- Serial log tags: `TIMER` prints `ax= ay= az=` and `ROT` status
- Common crash: WiFi driver memory exhaustion (`wifi:alloc eb fail`) → reduce LVGL buffer or increase PSRAM
- Rotation artifacts: `st7789_clear()` allocates 134KB full-screen black pixels from PSRAM and flushes once
- Quick hardware check: I2C scan addresses (AHT20=0x38, LSM6DS3=0x6A, ES8311=0x18, ES7210=0x40)

# Critical Gotchas

## MSYS2 / Build Environment
- ESP-IDF refuses to run under MSYS2/Mingw. Always use cmd.exe or PowerShell.
- Set `$env:MSYSTEM=''` before running idf.py from a PowerShell launched from bash.
- Parallel builds on Windows can fail intermittently (`cc1.exe: CreateProcess: No such file or directory`). Retry or limit with `ninja -j2`.

## sdkconfig
- **Never delete `build/` without backing up `sdkconfig`** — `sdkconfig.defaults` alone doesn't restore LVGL fonts, partition scheme, or PSRAM config.
- `idf.py set-target esp32s3` regenerates from `sdkconfig.defaults` which is incomplete.
- XiaoZhi AI must use the same `partitions.csv` (app linked at 0x810000). XiaoZhi side disables OTA (would write wrong slot).

## PSRAM Memory
- Use `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (default). **Never set `CONFIG_SPIRAM_USE_MALLOC=y`** — DMA components (mbedTLS, I2S, WiFi) will crash.
- NES SCREENMEMORY (61KB) must use `MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT`, not `MALLOC_CAP_DMA`.
- `managed_components/` is auto-generated — do not commit. `dependencies.lock` should be committed.

## Component Manager
- `IDF_COMPONENT_MANAGER=1` required for zlib (`espressif/zlib: ^1.3.1` in `main/idf_component.yml`). ESP_BSP normally has it disabled — only enable when zlib is needed.
- The zlib dependency is needed by weather module (HeFeng API always returns gzip despite `&gzip=n`). Use `inflateInit2(&strm, 16 + MAX_WBITS)` for auto-detection.
- XiaoZhi AI **must** have `IDF_COMPONENT_MANAGER=1` (has 50+ managed component dependencies).

## SPI Bus Sharing
- SD card and LCD share SPI2_HOST. SD initializes first at 400kHz, LCD joins later at 60MHz (actual ~40MHz on ESP32-S3 due to APB=80MHz, min divider=2).

## NES Game
- Task WDT on Core 1 is expected (spin-wait starves idle task). `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n`.
- Exit: B+START held 3 frames → `s_running=false` → cleanup → `s_game_done_sem` → task delete.
- Stop during ROM loading flow: `nes_game_stop()` sets `s_abort=true`, `s_running=false`, waits on `s_game_done_sem`. game_task finishes loading → checks `s_abort` → `goto done` → gives semaphore → deletes.
- `nes_game_set_exit_callback(on_game_exit)` must be called in `nes_start()`, not `nes_wrapper_init()`.

## LVGL Memory
- LVGL heap: 64KB (`LV_MEM_SIZE`). After ~3-5 screen transitions without deleting old screens, allocation fails → freeze.
- Home screen data: cached in static variables (`home_screen.c`). On navigation return, `home_screen_create()` restores from cache.

## GCC 14+ Compatibility
- Use `%zu` for `size_t` in printf. C++ files need explicit `#include <cstring>`.
- `file(TO_CMAKE_PATH)` needs quotes around empty variables.
- Add new modules to both `SRCS` and `REQUIRES`/`INCLUDE_DIRS` in `main/CMakeLists.txt`.

## Hardware Pin Map

```
LCD:    BL=IO9  DC=IO10 CS=IO11 SCK=IO12 MOSI=IO13 RST=IO14
SD:     CS=IO18 MISO=IO8   (shared SCK/MOSI with LCD)
Audio:  MCLK=IO40 BCLK=IO39 WS=IO45 DOUT=IO48 DIN=IO21
        Codec I2C: SDA=IO38 SCL=IO47
Sensor: I2C SDA=IO2 SCL=IO1
Keys:   UP=IO5 DOWN=IO6 LEFT=IO4 RIGHT=IO7 A=IO17 B=IO16 START=IO15
BAT:    ADC=IO3 CHRG=IO41 DONE=IO42
LED:    IO18
Audio PA enable: IO46
```

# Code Conventions

- C99 for most code; C++ only for `components/nes_core/emucore.cpp` and `modules/wifi_manager/wifi_bridge.cpp`.
- Component dependencies in `main/CMakeLists.txt` REQUIRES list.
- `#include` paths use INCLUDE_DIRS from `main/CMakeLists.txt` (relative to `main/`).
- `.gitignore` excludes: `build/`, `managed_components/`, `sdkconfig`, `.claude/`.

# Project Sources / References

| Component | Source |
|-----------|--------|
| NES emulator core | https://github.com/planevina/esp32s3_nes_gamer (forked from NesCat) |
| WiFi manager | https://github.com/78/esp-wifi-connect |
| LVGL v8.4 | https://github.com/lvgl/lvgl |
| XiaoZhi AI | https://github.com/78/xiaozhi-esp32 |
| Audio codec dev | Espressif component registry (espressif/esp_codec_dev) |
| Weather API | HeFeng (devapi.qweather.com) |
| NTP | ntp.aliyun.com + pool.ntp.org |
