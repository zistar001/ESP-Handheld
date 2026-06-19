# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Build & Flash

**IMPORTANT:** Never build from MSYS2/Mingw bash. ESP-IDF detects `MSYSTEM` and refuses. Use cmd.exe or PowerShell.

## Environment (办公室 ↔ 家里)

Paths defined in `idf_env.ps1`. Before building, ask user which environment, then set paths accordingly.

| Environment | IDF_PATH | IDF_TOOLS_PATH | COM Port |
|-------------|----------|----------------|----------|
| 🏢 办公室 | `D:\esp\v5.5.4\esp-idf` | `C:\Espressif\tools` | COM3 |
| 🏠 家里 | `D:\Espressif\frameworks\esp-idf-v5.5.4` | `D:\Espressif\tools` | COM7 |

Common: target `esp32s3`, flash baud 921600.

## Canonical Build (PowerShell)

```powershell
$env:MSYSTEM=''
$env:IDF_PATH='D:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='0'   # set to '1' only for weather (zlib)
# Add cmake, ninja, toolchain, python to PATH (see _bf.ps1 for exact paths)
python $env:IDF_PATH\tools\idf.py build
```

**Note:** `IDF_COMPONENT_MANAGER=1` is required only for the weather module (zlib). Most builds don't need it — keep `0` for faster offline builds.

## One-Click Scripts

| Script | Purpose |
|--------|---------|
| `_bf.ps1` | **Primary** — build+flash (IDF v5.5.4, COM7) |
| `_flash_dualsystem.ps1` | Flash both ESP_BSP + XiaoZhi (full dual-system) |
| `_build.ps1` | Build only (IDF v5.4.1, COMPONENT_MANAGER=1) |
| `_build_v5_4.ps1` | Build only (IDF v5.4.1, no component manager) |
| `_flash.ps1` | Full flash to COM3 (legacy, v5.4.1) |
| `_flash_app.ps1` | App-only flash to COM3 (legacy) |
| `_monitor.ps1` | 3-second serial read on COM3 |
| `_verify.ps1` | Quick serial check on COM3 |
| `retro-go-firmware/build_rg.ps1` | Build retro-go standalone firmware |
| `tools/pc_voice_receiver.py` | PC-side voice receiver (WiFi audio) |
| `tools/send_ip.py` | Send IP to device |

## Retro-Go Game System (ota_0)

retro-go is a standalone firmware flashed to the **ota_0** partition (4MB, offset 0x410000).
It contains a ROM browser launcher + all emulator cores in a single binary.

### Build & Flash

```powershell
cd retro-go-firmware
powershell -ExecutionPolicy Bypass -File build_rg.ps1   # builds to build/retro-go-firmware.bin
esptool.py -p COM7 -b 921600 write_flash 0x410000 build/retro-go-firmware.bin
```

### ROM Paths (SD Card)

| Platform | Path |
|----------|------|
| NES | `/sdcard/roms/nes/*.nes` |
| GB/GBC | `/sdcard/roms/gb/*.gb` |
| SMS/GG | `/sdcard/roms/sms/*.sms` |
| PCE | `/sdcard/roms/pce/*.pce` |
| SNES | `/sdcard/roms/snes/*.smc` |

### 7-Button Combo Keys

| Physical | Function | Notes |
|----------|----------|-------|
| UP/DOWN/LEFT/RIGHT | D-pad | |
| A | Confirm / Action A | |
| B | Cancel / Action B | |
| START | Gamepad START | |
| **START + A** | **SELECT** | Chord combo |
| **START + B** (hold 150ms) | **Return to ESP_BSP** | Same as XiaoZhi |
| **START** (hold 1s) | **System Menu** | Save/Load/Exit |

### Supported Emulators

| Core | Platform | From |
|------|----------|------|
| nofrendo | NES/FDS | `D:\retro-go\retro-core\components\nofrendo\` |
| gnuboy | GB/GBC | `D:\retro-go\retro-core\components\gnuboy\` |
| smsplus | SMS/GG/Coleco | `D:\retro-go\retro-core\components\smsplus\` |
| pce-go | PCE/TG16 | `D:\retro-go\retro-core\components\pce-go\` |
| handy | Atari Lynx | `D:\retro-go\retro-core\components\handy\` |
| snes9x | SNES | `D:\retro-go\retro-core\components\snes9x\` |
| gw-emulator | Game & Watch | `D:\retro-go\retro-core\components\gw-emulator\` |

### Firmware Structure

```
retro-go-firmware/
├── CMakeLists.txt          ← Project def, references D:\retro-go components
├── main/
│   ├── main.c              ← app_main routing + rg_system_exit override
│   ├── launcher_integration.c  ← ROM browser UI loop
│   └── CMakeLists.txt
├── deps/retro-core-mains/  ← Emulator entry points (copied from D:\retro-go)
├── sdkconfig.defaults
└── build_rg.ps1
```

**Note:** There are TWO IDF versions — migrated from v5.4.1→v5.5.4. Legacy scripts target COM3/v5.4.1; primary scripts (`_bf.ps1`) target COM7/v5.5.4. Use `_bf.ps1` for new work.

## Full Flash (must include ota_data_initial.bin)

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

## Quick Flash (app-only)

```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 write_flash 0x10000 build/esp_handheld.bin
```

## SPIFFS Assets

```bash
# Build image
python $env:IDF_PATH/components/spiffs/spiffsgen.py 0x3F0000 assets/ build/assets.bin
# Flash
python -m esptool --chip esp32s3 -p COM7 -b 921600 write_flash 0xC10000 build/assets.bin
```

## Monitor

```bash
idf.py -p COM7 monitor   # 115200 baud, Ctrl+] to exit
```

## Flash XiaoZhi AI to ota_1 (dual-system)

```bash
esptool.py -p COM7 -b 921600 write_flash 0x810000 build/xiaozhi.bin
```
XiaoZhi source at `xiaozhi-esp32-main/`, must use the same `partitions.csv` (app linked at 0x810000).

---

# Project Architecture

ESP32-S3 handheld device firmware — NES emulator + LVGL UI + weather + BLE remote + WiFi audio + dual-firmware with XiaoZhi AI. Built with **ESP-IDF v5.5.4**, project name `esp_handheld`. Entry point: `main/main.c`.

**Hardware:** ST7789 240×280 SPI LCD, ES8311 audio, ES7210 mic, LSM6DS3TR-C IMU, AHT20 temp/humidity, 7 physical keys, 16MB flash, 8MB Octal PSRAM.

## Code Layout

```
main/
  main.c              — Entry: init, create tasks, key_handler dispatch
  bsp/                — Board support (st7789, sd_card, bsp_board, key_driver)
  app/                — App framework (app_manager, launcher, menu, rom_browser)
  modules/
    audio/            — ES8311 DAC + ES7210 ADC + box_audio_codec
    imu/              — LSM6DS3TR-C driver + calibration
    sensor/           — AHT20 temp/humidity
    weather/          — HeFeng weather API (HTTPS + zlib gunzip)
    pc_remote/        — BLE HID keyboard/mouse, air mouse, WiFi audio
    wifi_manager/     — WiFi connect/provisioning (esp-wifi-connect C++ bridge)
    power/            — Battery ADC monitor
    settings/         — NVS settings (volume, brightness, sleep, etc.)
    time_sync/        — SNTP + NVS backup
    iching/           — I Ching + Xiao Liu Ren fortune divination
  ui/
    display_driver.c  — LVGL init, double-buffer, lvgl_lock/lvgl_unlock
    screens/          — home, menu, settings, countdown, airmouse, kbd,
                        ip_input, fortune, liuren, calib, record
    components/       — status_bar, weather icons
components/
  lvgl/               — LVGL v8.4 (local source, not from registry)
  nes_core/           — NES emulator C++ core (from esp32s3_nes_gamer)
  esp-wifi-connect/   — WiFi provisioning (github.com/78/esp-wifi-connect)
  espressif__esp_codec_dev/ — Codec device abstraction
  lv_conf.h           — LVGL config (not Kconfig)
```

## Partition Layout (16MB flash - Three Systems)

| Partition | Offset | Size | Content |
|-----------|--------|------|---------|
| factory   | 0x10000  | 4MB | ESP_BSP (LVGL launcher, weather, settings, utilities) |
| ota_0     | 0x410000 | 4MB | **Retro-Go (standalone game system)** |
| ota_1     | 0x810000 | 4MB | XiaoZhi AI firmware |
| assets    | 0xC10000 | ~4MB | SPIFFS resources |

System switching via `esp_ota_set_boot_partition()` + `esp_restart()`. All three systems share WiFi config via NVS namespace `"wifi"` (esp-wifi-connect SsidManager).

**Boot flows:**
```
ESP_BSP menu → "游戏"  → esp_restart() → ota_0 (Retro-Go)
ESP_BSP menu → "小智"  → esp_restart() → ota_1 (XiaoZhi)
Retro-Go:    START+B  → esp_restart() → factory (ESP_BSP)
XiaoZhi:     START+B  → esp_restart() → factory (ESP_BSP)
```

## FreeRTOS Task Layout

| Task | Stack | Prio | Core | Role |
|------|-------|------|------|------|
| key | 2048 | 5 | 0 | Key scan (software debounce, 3-sample/10ms) + invokes key_handler |
| sensor | 3072 | 3 | 0 | AHT20 read + home screen update |
| imu | 3072 | 3 | 0 | IMU read + attitude calculation |
| airmouse | 3072 | 3 | 0 | Gyro→BLE mouse delta |
| batt | 3072 | 2 | 0 | Battery monitoring (10s interval) |
| pm | 2048 | 1 | 0 | Power mgmt, Light Sleep check (5s) |
| game_task | — | — | 1 | NES emulation (60fps loop) |
| video_task | — | — | 0 | NES frame SPI transfer (~38fps) |

## App State Machine

```
Launcher ←──[START]──→ Menu ←──[B/START]──→ Launcher
                          ↓ [A select]
                       Running (app)
                          ↓ [B/START]
                       Menu
```

- **Launcher** = home screen (weather, time, status bar). START → Menu.
- **Menu** = 2-column grid, 8 app cards (游戏/小智/键盘/鼠标/占卜/小六壬/计时/设置). Selection: 3px golden border (`0xFFBB00`) + 7% fill tint. Navigation stops at grid edges. Remembers last selection.
- **"游戏" card** = partition-switches to ota_0 (Retro-Go standalone firmware). No longer runs NES in-process.
- **Running** = the active app. B or START returns to Menu (most apps).
- **return-to-settings:** `app_manager_set_return_to(APP_ID_SETTINGS)` makes return go to settings instead of menu.

## Key Handler Dispatch

`key_handler()` in `main.c` is called from the key task (Core 0, prio 5). It's a central switch on `app_manager_get_state()`:

```
APP_STATE_LAUNCHER:
  START pressed → lvgl_lock → menu_enter()

APP_STATE_MENU:
  D-pad → lvgl_lock → menu_navigate(dx, dy)
  A     → lvgl_lock → menu_select()   // launches the selected app
  B/START → lvgl_lock → app_manager_return()

APP_STATE_RUNNING:
  switch (current_app):
    APP_ID_NES:       partition-switch to ota_0 (Retro-Go), no in-process handling
    APP_ID_KEYBOARD:  physical keys → HID codes, START+B=exit, A-hold=voice
    APP_ID_MOUSE:     A=left click, B=right click, UP/DOWN=sensitivity
    APP_ID_SETTINGS:  D-pad nav in list/sub-screens, A=select, B=back
    APP_ID_FORTUNE:   delegated to fortune_screen (no special key handling)
    APP_ID_RECORDER:  delegated to liuren_screen (A=trigger divination)
    APP_ID_COUNTDOWN: delegated to countdown_screen (START+B=exit)
    APP_ID_CALIB:     delegated to calib_screen
    default:          fall through to menu exit
```

**Pattern for adding a new app:** Add an `else if` block in `APP_STATE_RUNNING` checking `app_manager_get_current_app()`.

## LVGL Screen Lifecycle

LVGL has a 64KB heap. Screen transitions without cleanup exhaust it in 3-5 switches.

### Create/Delete Pattern

Every screen create function must:
1. Save old screen: `lv_obj_t *old = lv_scr_act();`
2. Create new screen: `lv_obj_t *scr = lv_obj_create(NULL);`
3. Setup UI widgets...
4. Delete old screen: `lv_obj_del(old);`

### Static Pointer Safety

Screens store widget pointers as statics. When the old screen is deleted on exit, those pointers **dangle**. Every `*_screen_create()` must reset all static pointers to NULL:

```c
static lv_obj_t *s_btn = NULL;
void foo_screen_create(void) {
    s_btn = NULL;   // MUST reset — dangled pointer on re-entry
    // ... create widgets, assign s_btn
}
```

**Failure to reset causes a crash on the second invocation** (Xiao Liu Ren has hit this).

### Thread Safety

LVGL calls from **outside the LVGL timer handler** must hold the mutex:
```c
lvgl_lock();
lv_obj_set_style_bg_color(some_btn, lv_color_hex(0xFF0000), 0);
lvgl_unlock();
```

- `key_handler` calls always need locking (runs on Core 0, prio 5)
- Sensor/IMU task callbacks updating labels need locking
- NES exit path: `nes_wrapper_check_exit()` is called from key_handler context, which is already considered safe

### Home Screen Cache

Home screen restores its state from static cache variables (`s_cached_*` in `home_screen.c`). The big weather icon is restored from `s_cached_desc` — it's NOT auto-updated from forecast data. Any new widget added to the home screen must be cached the same way.

## Retro-Go Game System

Games now run as a **standalone firmware on ota_0 partition** (retro-go). See the "Retro-Go Game System" section above for build, flash, and key mapping details.

**Key difference from old in-process NES:** The "游戏" menu card now calls `esp_ota_set_boot_partition("ota_0")` + `esp_restart()` instead of launching NES in-process. The retro-go firmware runs independently with its own display/audio/input drivers.

## How to Add a New App

1. **app_manager.h** — Add `APP_ID_FOO` to the `app_id_t` enum
2. **app_manager.c** — Add case in `app_manager_launch()` switch, call `foo_screen_create()`
3. **ui/screens/foo_screen.c/h** — Create screen with create/destroy, handle input
4. **ui/screens/menu_screen.c** — Add entry to `items[]` array (icon + label + app_id)
5. **main/CMakeLists.txt** — Add source files to `SRCS` and include dirs to `INCLUDE_DIRS`
6. **main.c** — Add `#include "ui/screens/foo_screen.h"` and key handling in `key_handler()` under `APP_STATE_RUNNING`
7. **menu screen "item_count"** — If your `menu_screen.c` uses a hardcoded `item_count`, update it

## Audio Subsystem

I2S_NUM_0 shared bus: TX (standard) → ES8311 DAC → speaker, RX (TDM 4-slot) → ES7210 ADC → dual mic. 16kHz, 16bit. Volume/mic gain NVS-persisted.

## BLE HID

Bluedroid BLE, HID keyboard + mouse. Just Works pairing, device name "ESP-Handheld". Passkey hardcoded "1234". API: `ble_hid_send_key(modifier, key)`, `ble_hid_send_mouse(buttons, dx, dy)`.

## WiFi Manager

Uses `78/esp-wifi-connect` v3.1.4 (C++, bridged to C via `wifi_bridge.cpp`). Soft-AP + DNS hijack + built-in web config page. Multi-SSID storage via SsidManager. Auto-migrates old settings_manager WiFi data.

---

# Dual-Firmware System

## ESP_BSP → XiaoZhi
Menu → 小智 card → detects current partition and toggles: factory/ota_0→ota_1, ota_1→factory. Calls `esp_ota_set_boot_partition()` + `esp_restart()`.

## XiaoZhi → ESP_BSP
**START+B** (GPIO15+GPIO16) held for ~150ms → `ReturnToHandheld()` → reboots to factory.

## XiaoZhi Side Button Map

| Key | GPIO | XiaoZhi Function |
|-----|------|-----------------|
| **A** | 17 | Push-to-Talk (hold to speak) |
| **UP** | 5 | Volume + (click +10, long-press max) |
| **DOWN** | 6 | Volume - (click -10) |
| **START** | 15 | Combo: START+B = return to ESP_BSP |
| **B** | 16 | Same combo |
| LEFT/RIGHT | 4,7 | Unused |

## WiFi Sharing

Both systems use the same `esp-wifi-connect` SsidManager with NVS namespace `"wifi"`. Credentials configured in either system are available to the other automatically.

---

# Critical Gotchas

## Build Environment
- ESP-IDF refuses to run under MSYS2/Mingw. Use cmd.exe or PowerShell.
- Set `$env:MSYSTEM=''` before running idf.py from a PowerShell launched from bash.
- Parallel builds on Windows can fail intermittently (`cc1.exe: CreateProcess: No such file or directory`). Retry or limit with `ninja -j2`.
- If `idf.py build` fails with "component_manager: unknown command", ensure `IDF_COMPONENT_MANAGER` is set correctly for the build.

## sdkconfig
- **Never delete `build/` without backing up `sdkconfig`** — `sdkconfig.defaults` alone doesn't restore LVGL fonts, partition scheme, or PSRAM config.
- `idf.py set-target esp32s3` regenerates from `sdkconfig.defaults` which is incomplete.

## PSRAM Memory
- `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (default). **Never set `CONFIG_SPIRAM_USE_MALLOC=y`** — DMA components (mbedTLS, I2S, WiFi) crash.
- NES SCREENMEMORY (61KB) must use `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`, **not** `MALLOC_CAP_DMA`.
- `managed_components/` is auto-generated — do not commit. `dependencies.lock` should be committed.

## Component Manager
- `IDF_COMPONENT_MANAGER=1` required for zlib (`espressif/zlib: ^1.3.1` in `main/idf_component.yml`). ESP_BSP normally has it disabled — only enable for weather builds.
- The zlib dependency is needed by weather module (HeFeng API always returns gzip despite `&gzip=n`). Use `inflateInit2(&strm, 16 + MAX_WBITS)` for auto-detection.

## SPI Bus Sharing
- SD card and LCD share SPI2_HOST. SD initializes first at 400kHz, LCD joins later at 60MHz (actual ~40MHz on ESP32-S3 due to APB=80MHz, min divider=2).
- If LCD shows garbage after SD card activity, check that `SD_CS` (GPIO18) is properly driven high when not accessing the SD card.

## Retro-Go Firmware Notes

### Memory
- Frame buffers and surfaces use `MEM_SLOW` (PSRAM) to avoid exhausting internal SRAM.
- `app_main()` calls `heap_caps_malloc_extmem_enable(1024)` so all `malloc()` calls from emulator cores go to PSRAM.
- Retro-go's DMA buffers for SPI display are allocated from `MEM_DMA` (internal SRAM) — keep these small.
- Boot config (`boot.json`) stored on SD card at `/sdcard/retro-go/config/`.

### Boot Config Reliability
- Boot config (which emulator + ROM to launch) is saved to SD card via `rg_settings`.
- On `rg_system_exit()`, the config file is `unlink()`ed and the partition is switched to factory (ESP_BSP).
- A 1.5s delay ensures SD card writes complete before `esp_restart()`.

### Display
- ST7789 240x280, driver 0 (ILI9341/ST7789 direct SPI).
- COLMOD=0x55 (RGB565), MADCTL from `RG_SCREEN_RGB_BGR` and `RG_SCREEN_ROTATION`.
- Vertical gap `RG_GAP_Y` in ili9341.h adjusts for ST7789 glass misalignment.
- See `components/retro-go/targets/esp-bsp-handheld/config.h` for pin mappings.

## LVGL Memory
- LVGL heap: 64KB (`LV_MEM_SIZE`). After ~3-5 screen transitions without deleting old screens, allocation fails → freeze.
- Home screen data: cached in static variables (`home_screen.c`). On navigation return, `home_screen_create()` restores from cache.
- All LVGL calls from tasks (key_handler, sensor, etc.) must use `lvgl_lock()/lvgl_unlock()`.

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

## .gitignore Policies

| Excluded | Reason |
|----------|--------|
| `build/` | Build artifacts |
| `managed_components/` | Auto-generated (but `dependencies.lock` is committed) |
| `sdkconfig` | Generated from `sdkconfig.defaults` + menuconfig |
| `.claude/` | Session data, memory |
| `*.txt` | Temp/log files |
| `xiaozhi-esp32-main/managed_components/`, `xiaozhi-esp32-main/build/` | External repo artifacts |
| `retro-go-esp32s3-st7789v-v1.1/`, `esp32s3_nes_gamer/`, `Rachel/` | Unused reference repos |

---

# Debugging Guide

## Memory Debugging

```c
// Check remaining DMA memory (critical for WiFi/audio)
ESP_LOGI(TAG, "DMA free: %zu", heap_caps_get_free_size(MALLOC_CAP_DMA));

// Check PSRAM usage
ESP_LOGI(TAG, "PSRAM total: %zu, free: %zu",
    heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
    heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

// Dump full heap info
heap_caps_print_heap_info(MALLOC_CAP_8BIT);

// LVGL heap monitor
lv_mem_monitor_t mm;
lv_mem_monitor(&mm);
printf("LVGL heap: used=%d, free=%d, frag=%d%%\n", mm.used_size, mm.free_size, mm.frag_pct);
```

## Hardware Diagnostics

- **I2C probe:** `i2c_master_probe(I2C_NUM_0, 0x38, 100)` — returns ESP_OK if device at address responds
- **I2C address map:** AHT20=0x38, LSM6DS3=0x6A, ES8311=0x18, ES7210=0x40
- **PSRAM verification:** `heap_caps_get_total_size(MALLOC_CAP_SPIRAM)` should return ~8MB. Returns 0 if PSRAM is not initialized.

## Common Crashes

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `wifi:alloc eb fail` | WiFi driver out of DMA memory | Reduce LVGL buffer size or increase PSRAM allocation for WiFi |
| LVGL freeze after 3-5 screen switches | LVGL 64KB heap exhausted from undeleted screens | Check for missing `lv_obj_del(old_scr)` in screen create functions |
| Crash on 2nd entry to a screen | Dangling static pointer from previous `lv_obj_del` | Reset all static widget pointers to NULL in `*_screen_create()` |
| `Guru Meditation Error` in game_task | Stack overflow on Core 1 (game_task) | Increase game_task stack or reduce stack usage in ROM loading |
| cJSON_Parse returns NULL in weather | No PSRAM available for cJSON tree | Check `cjson_malloc` hooks force `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` |
| LCD horizontal stripes | Single LVGL buffer, not double | Must have 2 draw buffers in PSRAM (DMA sends A while LVGL renders B) |

## Serial Log Tags

| Tag | Context |
|-----|---------|
| `MAIN` | Entry, key handler dispatch |
| `APP_MGR` | App lifecycle (launch/return) |
| `TIMER` | IMU readings (`ax= ay= az=`) + rotation status |
| `WEATHER` | Weather fetch, parse, cache |
| `NES` | Game lifecycle, frame timing |
| `WIFI` | Connection state, provisioning |
| `LVGL` | Display driver, buffer swaps |

## Weather Module Tips

- HeFeng API key: `700cf8ab08774bf089e52d33b89aecf8`, Default Location: `101230501` (泉州), Host: `p23p3qvugk.re.qweatherapi.com`
- **Location is now user-configurable** via Settings → 天气地区. Stored in NVS namespace `"weather"` (keys `location_id`, `location_name`). 13 Quanzhou districts available, some share the same API ID but display name is preserved.
- Location change triggers `xTaskNotifyGive()` to wake weather task for immediate refresh.
- API always returns gzip (ignores `&gzip=n`). Must use `inflateInit2(&strm, 16 + MAX_WBITS)`.
- cJSON allocators must force PSRAM with `MALLOC_CAP_8BIT` or they return NULL silently.
- Weather data processes regardless of current screen. Home screen restores from `s_cached_*` static vars.

---

# Code Conventions

- C99 for most code; C++ only for `components/nes_core/emucore.cpp` and `modules/wifi_manager/wifi_bridge.cpp`.
- Component dependencies in `main/CMakeLists.txt` REQUIRES list.
- `#include` paths use INCLUDE_DIRS from `main/CMakeLists.txt` (relative to `main/`).
- Settings stored in NVS (`settings` namespace), loaded via `settings_load(settings_t *s)`.

# Project Sources

| Component | Source |
|-----------|--------|
| Retro-Go (game system) | https://github.com/ducalex/retro-go — multi-emulator system on ota_0 |
| NES core (nofrendo) | Included in retro-go (`retro-core/components/nofrendo/`) |
| WiFi manager | https://github.com/78/esp-wifi-connect |
| LVGL v8.4 | https://github.com/lvgl/lvgl |
| XiaoZhi AI | https://github.com/78/xiaozhi-esp32 |
| Audio codec dev | Espressif component registry (espressif/esp_codec_dev) |
| Weather API | HeFeng (devapi.qweather.com) |
| NTP | ntp.aliyun.com + pool.ntp.org |
