# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**ESP32-S3 Handheld Firmware** — LVGL UI + weather + BLE HID remote + WiFi audio + in-process NES emulator + dual-firmware with XiaoZhi AI. Built with **ESP-IDF v5.5.4**, project name `esp_handheld`. Entry point: `main/main.c`.

<!-- TOC -->
- [Quick Reference](#quick-reference)
- [Build & Flash](#build--flash)
- [Retro-Go Game System (ota_0)](#retro-go-game-system-ota_0)
- [Project Architecture](#project-architecture)
- [Screen System](#screen-system)
- [Theme System](#theme-system)
- [NVS Namespace Map](#nvs-namespace-map)
- [Dual-Firmware System](#dual-firmware-system)
- [Audio, BLE, IMU, WiFi](#audio-ble-imu-wifi)
- [How to Add a New App](#how-to-add-a-new-app)
- [How to Add a New Component](#how-to-add-a-new-component)
- [Module Status Map](#module-status-map)
- [Critical Gotchas](#critical-gotchas)
- [Debugging Guide](#debugging-guide)
- [Hardware Reference](#hardware-reference)
- [Project Sources](#project-sources)

---

# Quick Reference

| Action | Command |
|--------|---------|
| Build | `idf.py build` (after setting up ESP-IDF) |
| Flash app only | `python -m esptool --chip esp32s3 -p (PORT) -b 921600 write_flash 0x10000 build/esp_handheld.bin` |
| Full flash | See [Full Flash section](#full-flash-must-include-ota_data_initialbin) |
| Monitor | `idf.py -p (PORT) monitor` (115200 baud, Ctrl+] to exit) |
| Configure API keys | `idf.py menuconfig` → "ESP-Handheld Configuration" |
| Build retro-go | See [Retro-Go section](#retro-go-game-system-ota_0) |

**IMPORTANT:** Never build from MSYS2/Mingw bash. ESP-IDF detects `MSYSTEM` and refuses. Use cmd.exe or PowerShell.

## First-Time Setup

```powershell
git clone --recursive <repo-url>
# If already cloned without --recursive:
git submodule update --init --recursive
```

Submodules: LVGL v8.4 (`components/lvgl`) and esp-wifi-connect (`components/esp-wifi-connect`).

The CJK font (`lv_font_simsun_16_cjk.c`) must be placed in `components/lvgl/src/font/`. Generate from `chinese_chars.txt` using `lv_font_conv` or restore from a prior build.

## Environment

Before building, set up ESP-IDF v5.5.4 for target `esp32s3`. Either use the official ESP-IDF installer or:

```powershell
# Clone ESP-IDF
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
# Run the installer
cd esp-idf
.\install.ps1 esp32s3
.\export.ps1    # or use the Windows GUI installer
```

Common config: target `esp32s3`, flash baud 921600, 16MB flash, 8MB Octal PSRAM.

## Canonical Build

```powershell
# Set up ESP-IDF environment first (see above)
$env:IDF_COMPONENT_MANAGER='0'   # '1' only for weather (zlib)
idf.py build
```

**`IDF_COMPONENT_MANAGER=1` is required only for the weather module (zlib).** Most builds don't need it — keep `0` for faster offline builds. When set to `1`, it downloads components from the registry; when `0`, only local components are used.

## One-Click Scripts

| Script | Purpose |
|--------|---------|
| `scripts/setup.ps1` | **Recommended** — one-click setup + build for Windows |
| `scripts/setup.sh` | One-click setup + build for Linux/macOS |
| `retro-go-firmware/build_rg.ps1` | Build retro-go standalone firmware |
| `tools/pc_voice_receiver.py` | PC-side voice receiver (WiFi audio) |
| `tools/send_ip.py` | Send IP to device |

## Full Flash (must include ota_data_initial.bin)

```bash
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

Replace `(PORT)` with your serial port (e.g. `COM3` on Windows, `/dev/ttyACM0` on Linux).

**Without 0xd000 (ota_data_initial.bin), the device boots into XiaoZhi AI (ota_1) instead of factory.**

## Quick Flash (app-only)

```bash
python -m esptool --chip esp32s3 -p (PORT) -b 921600 write_flash 0x10000 build/esp_handheld.bin
```

## Flash XiaoZhi AI (zistar) to ota_1

```bash
# Build zistar board in xiaozhi-zistar-main first
cd xiaozhi-zistar-main
idf.py build
# Flash only the app binary to ota_1 partition
esptool.py -p (PORT) -b 921600 write_flash 0x810000 build/xiaozhi.bin
```

## SPIFFS Assets

```bash
python $env:IDF_PATH/components/spiffs/spiffsgen.py 0x3F0000 assets/ build/assets.bin
python -m esptool --chip esp32s3 -p (PORT) -b 921600 write_flash 0xC10000 build/assets.bin
```

## Monitor

```bash
idf.py -p (PORT) monitor   # 115200 baud, Ctrl+] to exit
```

---

# Retro-Go Game System (ota_0)

Retro-go is a standalone firmware flashed to the **ota_0** partition (4MB, offset 0x410000). It contains a ROM browser launcher + all emulator cores in a single binary.

## Build & Flash

```powershell
cd retro-go-firmware
powershell -ExecutionPolicy Bypass -File build_rg.ps1
esptool.py -p (PORT) -b 921600 write_flash 0x410000 build/retro-go-firmware.bin
```

## Firmware Structure

```
retro-go-firmware/
├── CMakeLists.txt              — Project def, refs emulator cores
├── main/
│   ├── main.c                  — app_main routing + rg_system_exit override
│   ├── launcher_integration.c  — ROM browser UI loop
│   └── CMakeLists.txt
├── deps/retro-core-mains/      — Emulator entry points
├── sdkconfig.defaults
└── build_rg.ps1
```

Only NES (nofrendo) emulator is in the main ESP_BSP `components/retro-core/`. Other emulators (gnuboy, smsplus, pce-go, handy, snes9x, gw-emulator) are referenced via emulator core paths in the retro-go firmware CMakeLists.txt — they are NOT part of the ESP_BSP build and only run on ota_0.

## ROM Paths (SD Card)

| Platform | Path |
|----------|------|
| NES | `/sdcard/roms/nes/*.nes` |
| GB/GBC | `/sdcard/roms/gb/*.gb` |
| SMS/GG | `/sdcard/roms/sms/*.sms` |
| PCE | `/sdcard/roms/pce/*.pce` |
| SNES | `/sdcard/roms/snes/*.smc` |

## 7-Button Combo Keys

| Physical | Function | Notes |
|----------|----------|-------|
| UP/DOWN/LEFT/RIGHT | D-pad | |
| A | Confirm / Action A | |
| B | Cancel / Action B | |
| START | Gamepad START | |
| **START + A** | **SELECT** | Chord combo |
| **START + B** (hold 150ms) | **Return to ESP_BSP** | Same as XiaoZhi |
| **START** (hold 1s) | **System Menu** | Save/Load/Exit |

Retro-go config at `components/retro-go/targets/esp-bsp-handheld/config.h`. Pin mappings match ESP_BSP.

---

# Project Architecture

## Hardware (Rev 2 PCB)

ST7789 240×280 SPI LCD (SPI2_HOST), SD card on independent SPI3_HOST, MAX98357 digital amp (I2S0 TX), digital mic (I2S1 RX), LSM6DS3TR-C IMU + AHT20 temp/humidity (shared I2C), 7 physical keys, 16MB flash, 8MB Octal PSRAM.

## Code Layout

```
main/
  main.c              — Entry: init, create tasks, key_handler dispatch
  bsp/                — Board support (st7789, sd_card, bsp_board, key_driver)
  app/                — App framework (app_manager, launcher, menu, rom_browser)
  modules/
    audio/            — MAX98357 DAC + MSM261 digital mic (I2S0 TX + I2S1 RX)
    imu/              — LSM6DS3TR-C driver + calibration
    sensor/           — AHT20 temp/humidity
    weather/          — HeFeng weather API (HTTPS + zlib gunzip)
    pc_remote/        — BLE HID keyboard/mouse, air mouse, WiFi audio
    wifi_manager/     — WiFi connect/provisioning (esp-wifi-connect C++ bridge)
    wifi_provisioning/— Thin wrapper forwarding to wifi_manager (legacy compatibility)
    power/            — Battery ADC monitor
    settings/         — NVS settings (volume, brightness, sleep, etc.)
    time_sync/        — SNTP + NVS backup
    iching/           — I Ching + Xiao Liu Ren fortune divination
    theme/            — Theme manager (6 themes, 5 color slots, NVS, mix-and-match)
    spectrum/         — Float32 FFT spectrum visualizer (mirrored bars, adjustable params)
    retro_go/         — rg_adapter.c: in-process retro-go emulator entry (NES only, longjmp-based)
    nes/              — STUB: legacy nes_wrapper, nes_game, font8x16 (no longer compiled)
  ui/
    display_driver.c  — LVGL init, double-buffer, lvgl_lock/lvgl_unlock
    screens/          — home, menu, settings, countdown, airmouse, kbd, ip_input,
                        fortune, liuren, calib, record, theme, spectrum
    components/       — status_bar, weather icon rendering, theme_colors macros
components/
  lvgl/               — LVGL v8.4 (git submodule, local source, not from registry)
  nes_core/           — NES emulator C++ core (DISABLED — CMakeLists.txt.disabled)
  retro-core/         — nofrendo NES emulator C source (from retro-go, compiled in-process)
  retro-go/           — Retro-Go runtime: display/input/audio drivers + gamepad mapping
  esp-wifi-connect/   — WiFi provisioning (git submodule, github.com/78/esp-wifi-connect)
  espressif__esp_codec_dev/ — Codec device abstraction (managed component)
  lv_conf.h           — LVGL config (not Kconfig)
```

**Migration state:** NES games previously ran in-process via `components/nes_core/` (C++, CMakeLists.txt.disabled) and `main/modules/nes/` (wrapper stubs). They now run either as standalone firmware on ota_0 (retro-go, for all emulators) or in-process via `components/retro-core/` (nofrendo, C only) through `main/modules/retro_go/rg_adapter.c`. The `modules/nes/` and `components/nes_core/` are dead code — kept for reference only.

## Partition Layout (16MB flash)

| Partition | Offset | Size | Content |
|-----------|--------|------|---------|
| nvs | 0x9000 | 16K | NVS storage |
| otadata | 0xD000 | 8K | OTA boot selection |
| phy_init | 0xF000 | 4K | PHY init data |
| factory | 0x10000 | 4MB | ESP_BSP (LVGL launcher, weather, settings, utilities) |
| ota_0 | 0x410000 | 4MB | **Retro-Go (standalone game system)** |
| ota_1 | 0x810000 | 4MB | XiaoZhi AI firmware |
| assets | 0xC10000 | ~4MB | SPIFFS resources |

System switching via `esp_ota_set_boot_partition()` + `esp_restart()`. All three systems share WiFi config via NVS namespace `"wifi"` (esp-wifi-connect SsidManager).

```
ESP_BSP menu → "游戏"  → esp_restart() → ota_0 (Retro-Go)
ESP_BSP menu → "小智"  → esp_restart() → ota_1 (XiaoZhi)
Retro-Go:    START+B  → esp_restart() → factory (ESP_BSP)
XiaoZhi:     START+B  → esp_restart() → factory (ESP_BSP)
```

## FreeRTOS Task Layout

Tasks created in `main/main.c` and `main/app/launcher.c`:

| Task | Stack | Prio | Core | Role |
|------|-------|------|------|------|
| key | 4096 | 2 | 0 | Key scan (software debounce, 3-sample/10ms) + invokes key_handler |
| airmouse | 3072 | 3 | 0 | Gyro→BLE mouse delta (if IMU + BLE ready) |
| pm | 4096 | 1 | 0 | Power mgmt, Light Sleep check (5s interval) |
| launcher_upd | 4096 | 1 | 0 | Home screen 1s tick: update time, battery, WiFi, indoor temp/humidity |

Plus internal tasks: weather (periodic HTTP fetch), WiFi (event loop, provisioning), BLE (Bluedroid stack).

The retro-go firmware (ota_0) has its own layout: video SPI transfer on Core 0, emulation on Core 1, audio on dedicated timer.

## App State Machine

```
Launcher ←──[START]──→ Menu ←──[B/START]──→ Launcher
                          ↓ [A select]
                       Running (app)
                          ↓ [B/START]
                       Menu
```

- **Launcher** = home screen (weather, time, status bar, indoor temp/humidity). START → Menu.
- **Menu** = 2-column scrollable grid, 9 app cards. Golden highlight border (`CLR_HIGHLIGHT`, default `0xFFBB00`) + 7% fill tint. Remembers last selection via static `sel` variable.
- **"游戏" card** = partition-switches to ota_0. **"小智" card** = partition-switches to ota_1 (toggles: factory/ota_0→ota_1, ota_1→factory).
- **Running** = active app. B or START returns to Menu.
- **`app_manager_set_return_to(APP_ID_SETTINGS)`** makes next return go to settings (used by WiFi reset flow).

## Init Sequence (`app_main` in `main/main.c`)

Boot order matters — SD card must init first to claim SPI bus:

1. **NVS** — `nvs_flash_init()` (erase+retry on corruption)
2. **Settings + Theme** — `settings_init()`, `theme_load()` from NVS
3. **SD Card** — `sd_card_init()` creates SPI3_HOST bus
4. **BSP board** — `bsp_board_init()` initializes LCD on SPI2_HOST, sets backlight
5. **LVGL** — `ui_display_init()` double-buffer (DMA fallback to PSRAM)
6. **Audio** — `box_audio_init()` MAX98357 + MSM261 digital mic, plays beep
7. **Sensors** — AHT20 (temp/humidity), LSM6DS3TR-C IMU (best-effort)
8. **Battery monitor** — `battery_monitor_init()` (may fail on Rev 2 — no ADC)
9. **BLE HID** — `ble_hid_init()` starts advertising as "ESP-Handheld"
10. **Air mouse task** — `air_mouse_init()` + `xTaskCreatePinnedToCore` (starts disabled)
11. **WiFi manager** — `wifi_manager_init()` (independent, no blocking)
12. **Weather + Time sync** — background HTTP + SNTP
13. **Key driver** — `key_driver_init()` creates key scan task (Core 0, prio 2)
14. **Power management** — `pm_task` sleep monitor (5s interval)
15. **App manager** — `app_manager_init()` shows launcher screen

## Key Handler Dispatch

`key_handler()` in `main.c` is called from the key task (Core 0, prio 2). Central switch on `app_manager_get_state()`:

**Launcher:** START → `lvgl_lock` → `menu_enter()`

**Menu:** D-pad → `menu_navigate(dx, dy)`, A → `menu_select()`, B/START → `app_manager_return()`

**Running (per-app dispatch):**

| App ID | Key Behavior |
|--------|-------------|
| `APP_ID_NES` | Partition-switch to ota_0 |
| `APP_ID_XIAOZHI` | Partition-switch to ota_1 / toggle back to factory |
| `APP_ID_KEYBOARD` | Physical keys → HID codes, START+B=exit, A=Enter, RIGHT=PTT voice |
| `APP_ID_MOUSE` | A=left click, B=right click, UP/DOWN=sensitivity, START+B=exit |
| `APP_ID_SETTINGS` | D-pad nav in list/sub-screens, A=select, B/START=back/exit |
| `APP_ID_FORTUNE` | D-pad nav, A=select divination type |
| `APP_ID_RECORDER` | D-pad nav, A=trigger Xiao Liu Ren calculation |
| `APP_ID_COUNTDOWN` | START+B combo=exit, A=reset when finished |
| `APP_ID_CALIB` | A=trigger IMU calibration, B/START=exit |
| `APP_ID_THEME` | UP/DOWN=nav, A=select/enter, B/START=exit |
| `APP_ID_SPECTRUM` | UP/DOWN=select param, LEFT/RIGHT/A=adjust, B/START=exit |
| `APP_ID_IP_INPUT` | UP/DOWN=nav, A=select, B/START=cancel+exit |
| `APP_ID_WIFI_SETUP` | A=reconfigure (deinits BLE, starts WiFi AP), B/START=exit |
| default | B/START → return to menu (catch-all) |

**Pattern for adding a new app:** Add an `else if` block in `APP_STATE_RUNNING` checking `app_manager_get_current_app()`.

## Power Management

The `pm_task` (5s interval) implements **Light Sleep**:

1. After inactivity timeout (default 300s), turns off backlight
2. **Deinits BLE** (`ble_hid_deinit()`) — releases BT controller
3. **Stops WiFi** (`wifi_manager_stop_station()`) — shuts down RF
4. Enters **Light Sleep** via `esp_light_sleep_start()` with EXT1 GPIO wake-up on all 7 key pins
5. On any key press → wakes, re-inits BLE, restarts WiFi, restores backlight

Estimated savings: ~150-200mA → ~5-10mA during standby.

Light Sleep skips when `APP_ID_COUNTDOWN` or `APP_ID_SPECTRUM` is active.

---

# Screen System

## LVGL Lifecycle

LVGL has a 64KB heap. Screen transitions without cleanup exhaust it in 3-5 switches.

### Create/Delete Pattern

Every screen create function must:
1. Save old screen: `lv_obj_t *old = lv_scr_act();`
2. Create new: `lv_obj_t *scr = lv_obj_create(NULL);`
3. Setup widgets...
4. Delete old: `lv_obj_del(old);`

### Static Pointer Safety

Screens store widget pointers as statics. After `lv_obj_del()`, those pointers **dangle**:

```c
static lv_obj_t *s_btn = NULL;
void foo_screen_create(void) {
    s_btn = NULL;   // MUST reset — dangled pointer on re-entry
}
```

### Thread Safety

LVGL calls from **outside the LVGL timer handler** must hold the mutex:

```c
lvgl_lock();
lv_obj_set_style_bg_color(some_btn, lv_color_hex(0xFF0000), 0);
lvgl_unlock();
```

Requires locking: `key_handler` calls (Core 0, prio 2), sensor/IMU callbacks, `launcher_upd` task.

### Home Screen Cache

Home screen restores from static cache variables (`s_cached_*` in `home_screen.c`). The big weather icon restores from `s_cached_desc` — it's NOT auto-updated from forecast data. Any new widget must be cached the same way.

## Settings SubScreen Architecture

Uses integer `s_sub_mode`:
- `0`: main settings list (volume, brightness, WiFi, weather location, theme, sleep, about, etc.)
- `> 0`: specific sub-screen (volume slider, location picker, key remapping)
- Navigation: `settings_screen_navigate(delta)` moves selection; `settings_screen_select()` enters sub-screen; `settings_screen_back()` returns from sub-screen (returns `true` if handled, `false` if at main list → caller should exit settings)

## Screen File Reference

| Screen | Files | App ID | Key Behavior |
|--------|-------|--------|-------------|
| Home | `home_screen.c/.h` | Launcher state | Auto-refreshed by launcher_upd task |
| Menu | `menu_screen.c/.h`, `menu.h/.c` | Menu state | 9 cards, 2-column grid |
| Settings | `settings_screen.c/.h` | `APP_ID_SETTINGS` | Sub-screen arch, NVS-backed |
| Keyboard | `kbd_screen.c/.h` | `APP_ID_KEYBOARD` | HID key codes + PTT voice |
| Air Mouse | `airmouse_screen.c/.h` | `APP_ID_MOUSE` | Mouse mode toggle |
| Countdown | `countdown_screen.c/.h` | `APP_ID_COUNTDOWN` | START+B exit, A reset |
| Fortune | `fortune_screen.c/.h` | `APP_ID_FORTUNE` | I Ching hexagram |
| Liu Ren | `liuren_screen.c/.h` | `APP_ID_RECORDER` | Xiao Liu Ren |
| Calibration | `calib_screen.c/.h` | `APP_ID_CALIB` | IMU calibration |
| Theme | `theme_screen.c/.h` | `APP_ID_THEME` | Theme + slot override |
| Spectrum | `spectrum_screen.c/.h` | `APP_ID_SPECTRUM` | Adjustable FFT params |
| IP Input | `ip_input.c/.h` | `APP_ID_IP_INPUT` | PC IP entry |
| Record | `record_screen.c/.h` | (not in menu) | WiFi audio toggle |
| WiFi Setup | Built into `app_manager.c` | `APP_ID_WIFI_SETUP` | Inline LVGL screen |
| About | Built into `app_manager.c` | `APP_ID_ABOUT` | System info |

---

# Theme System

Runtime color switching via NVS `tconf` namespace. All UI screens support it via `CLR_*` macros.

## Color Slots

| Slot | Usage | Example |
|------|-------|---------|
| `TSLOT_BG` | Background | `0x0A0A0A` |
| `TSLOT_ACCENT` | Active buttons, primary highlight | `0xFF5C00` |
| `TSLOT_HIGHLIGHT` | Menu selection border | `0xFFBB00` |
| `TSLOT_TEXT` | Primary text | `0xFFFFFF` |
| `TSLOT_SUBTEXT` | Secondary text, hints | `0x999999` |

## 6 Built-in Themes

| Theme | BG | ACCENT | HIGHLIGHT | TEXT | SUBTEXT |
|-------|----|--------|-----------|------|---------|
| 经典橙 (default) | `#0A0A0A` | `#FF5C00` | `#FFBB00` | `#FFFFFF` | `#999999` |
| 极光蓝 | `#0D1117` | `#0096FF` | `#58A6FF` | `#FFFFFF` | `#8B949E` |
| 赛博紫 | `#0A0015` | `#BB86FC` | `#00FFFF` | `#FFFFFF` | `#8870A0` |
| 翡翠绿 | `#0A0F0A` | `#00C853` | `#69F0AE` | `#FFFFFF` | `#A5D6A7` |
| 暗紫金 | `#120A1A` | `#BB86FC` | `#FFD700` | `#FFFFFF` | `#9980B0` |
| 钢铁灰 | `#121212` | `#FF6D00` | `#FF9100` | `#FFFFFF` | `#B0B0B0` |

## Usage

`#include "ui/components/theme_colors.h"` → `CLR_BG`, `CLR_ACCENT`, `CLR_HIGHLIGHT`, `CLR_TEXT`, `CLR_SUBTEXT` macros.

## Adding a New Theme

Add entry in `s_themes[]` in `modules/theme/theme_manager.c` and enum value in `theme_id_t` in header.

---

# NVS Namespace Map

| Namespace | Keys | Used By |
|-----------|------|---------|
| `settings` | `cfg` (blob: `settings_t`) | Volume, brightness, mic_gain, air_mouse_sens, sleep, key_map |
| `tconf` | `cfg` (blob: `theme_config_t`) | base_theme + 5 override colors |
| `wifi` | SSID entries via SsidManager | Shared by ESP_BSP + XiaoZhi |
| `weather` | `location_id`, `location_name` | User-configurable weather location |
| `time` | `tz_offset` | Time sync offset (s) |
| `ble` | `bt_hid_cfg` | BLE HID config |
| `voice` | `pc_octet` | WiFi audio PC IP last octet |
| `rg` | various | Retro-Go (boot config on SD card, not NVS) |

## `settings_t` Struct (`modules/settings/settings_manager.h`)

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `volume` | `uint8_t` | 0-100 | Audio output volume |
| `brightness` | `uint8_t` | 0-100 | LCD backlight |
| `mic_gain` | `uint8_t` | 0-100 | Microphone gain multiplier |
| `air_mouse_sens` | `float` | 0.1-5.0 | Gyro-to-mouse sensitivity |
| `wifi_ssid` | `char[32]` | — | Last connected SSID (legacy) |
| `wifi_pass` | `char[64]` | — | Associated password (legacy) |
| `sleep_enabled` | `bool` | — | Auto-sleep toggle |
| `sleep_timeout_sec` | `uint16_t` | 5-7200 | Inactivity timeout before screen-off |
| `key_map[7]` | `uint8_t[7]` | — | Key remapping indices |

---

# Dual-Firmware System

- **ESP_BSP → XiaoZhi:** Menu → 小智 card → detects current partition and toggles: factory/ota_0→ota_1, ota_1→factory. Calls `esp_ota_set_boot_partition()` + `esp_restart()`.
- **XiaoZhi → ESP_BSP:** **START+B** (GPIO15+GPIO16) held for ~150ms → `ReturnToHandheld()` → reboots to factory.

## XiaoZhi Side Button Map

| Key | GPIO | XiaoZhi Function |
|-----|------|-----------------|
| A | 17 | Push-to-Talk (hold to speak) |
| UP | 5 | Volume + |
| DOWN | 6 | Volume - |
| START | 15 | Combo: START+B = return to ESP_BSP |
| B | 16 | Same combo |
| LEFT/RIGHT | 4,7 | Unused |

## WiFi Sharing

Both ESP_BSP and XiaoZhi use the same `esp-wifi-connect` SsidManager with NVS namespace `"wifi"`. Credentials configured in either system are available to the other automatically.

---

# Audio, BLE, IMU, WiFi

## Audio Subsystem (Rev 2 PCB)

两个独立I2S控制器，各自独立时钟。无外部音频编解码芯片。

- **I2S_NUM_0 (RX):** BCLK=IO11, WS=IO12, DATA=IO10 → MSM261 数字麦克风
- **I2S_NUM_1 (TX):** BCLK=IO46, LRC=IO9, DIN=IO3 → MAX98357 数字功放
- 16kHz, 16bit. Volume/mic gain NVS-persisted in `settings_t`.

ESP_BSP的 `box_audio_codec.c` 使用独立I2S通道。XiaoZhi 使用 `NoAudioCodecSimplex` 类。

**麦克风增益（软件放大）：**

```c
// 增益映射：0-37dB → 1.0x-70x
if (db >= 37) gain = 70.0f;
else if (db >= 30) gain = 35.0f;
else if (db >= 20) gain = 15.0f;
else if (db >= 10) gain = 5.0f;
else gain = 1.0f + db * 0.5f;
```

`box_audio_read()` 从 I2S 立体声读取左右声道，取**信号强的那个**。

**功放上电时序：** MAX98357 需要 BCLK 后 ~12ms 稳定。`init_i2s1_tx()` 在 `i2s_channel_enable()` 后加了 25ms 延迟。

**诊断：** 启动时 `box_audio_diag()` 打印 I2S RX 数据（看 `BOX_AUDIO` 标签）：
- `ret=ESP_OK` + `range` 非零 → 正常
- `range=[0..0]` → 麦克风无数据，可能需改 I2S 模式

## WiFi Audio Streaming Protocol

See `modules/pc_remote/wifi_audio.c` + `tools/pc_voice_receiver.py`.

- **Audio data:** UDP unicast to PC IP, port **9210**
- **Format:** 16kHz, 16-bit signed PCM, 256 samples/packet (16ms audio)
- **Task:** `audio_tx_task` on Core 1, prio 3, stack 4096
- **PTT control:** UDP port **9212**, sends `"start"` / `"stop"`
- **PC IP:** Set last octet via IP Input screen; full IP rebuilt from WiFi subnet
- **NVS:** Last octet saved in `voice` namespace, key `pc_octet`

## BLE HID

Bluedroid BLE, HID keyboard + mouse. Just Works pairing (`ESP_IO_CAP_NONE`), device name "ESP-Handheld".

API: `ble_hid_send_key(modifier, key)`, `ble_hid_send_mouse(buttons, dx, dy)`.

**Windows pairing tips:**
- Just Works works on mobile/tablet, but some Windows BT adapters require MITM pairing (look for `PASSKEY_NOTIF` logs)
- Delete all "ESP-Handheld" pairings, restart BT, retry
- Log tag: `BLE_HID` — look for `AUTH OK` or `AUTH ERROR`

## IMU (LSM6DS3TR-C)

I2C addr 0x6A (SA0=VDD), ODR=104Hz, ACC ±2g, GYRO ±2000dps.

**Orientation correction:** `imu_read()` negates `ax`, `ay`, `gx`, `gy` — PCB mounts IMU horizontally flipped 180° (X and Y axes inverted). `az` and `gz` uncorrected.

`imu_calib.c` provides 4-orientation attitude detection (UPRIGHT/LEFT/RIGHT/FLAT). Calibration saved to NVS via `calib_screen` (`APP_ID_CALIB`).

## WiFi Manager

Uses `78/esp-wifi-connect` v3.1.4. C++ component bridged to C via `wifi_bridge.cpp`. Soft-AP + DNS hijack + built-in web config page. Multi-SSID via SsidManager. Auto-migrates legacy WiFi data.

---

# How to Add a New App

1. **`app_manager.h`** — Add `APP_ID_FOO` to `app_id_t` enum
2. **`app_manager.c`** — Add case in `app_manager_launch()` switch, call `foo_screen_create()`
3. **`ui/screens/foo_screen.c/h`** — Create screen with create/destroy pattern, handle input
4. **`ui/screens/menu_screen.c`** — Add entry to `items[]` (icon + label + app_id); update `item_count`
5. **`main/CMakeLists.txt`** — Add source files to `SRCS`, include dirs to `INCLUDE_DIRS`
6. **`main/main.c`** — Add `#include` and key handling in `key_handler()` under `APP_STATE_RUNNING`
7. **Theme support** — Use `CLR_*` macros from `theme_colors.h`

# How to Add a New Component

1. **`components/foo/`** — Create directory with `CMakeLists.txt` registering as `idf_component_register(SRCS ...)`
2. **`main/CMakeLists.txt`** — Add `foo` to `REQUIRES` list
3. If the component directory is under `main/` (a module), add source files to `SRCS` and path to `INCLUDE_DIRS` in `main/CMakeLists.txt`

# Tests

There are currently **no unit or integration tests** in this project. The codebase relies on hardware-in-the-loop validation via serial monitor and manual testing. When adding code, verify with build + flash + monitor cycles.

---

# Module Status Map

| Module | Status | Notes |
|--------|--------|-------|
| `modules/audio/` | Active | MAX98357 + MSM261 digital mic, I2S0 TX + I2S1 RX |
| `modules/audio/es8311_driver.*` | **REMOVED** | Rev 1 legacy, no longer compiled |
| `modules/audio/es7210_driver.*` | **REMOVED** | Rev 1 legacy, no longer compiled |
| `modules/imu/` | Active | LSM6DS3TR-C, on-demand |
| `modules/sensor/` | Active | AHT20, polled from launcher_upd task |
| `modules/weather/` | Active | HeFeng API, background task, NVS location |
| `modules/pc_remote/` | Active | BLE HID, air mouse, WiFi audio |
| `modules/wifi_manager/` | Active | esp-wifi-connect C++ bridge |
| `modules/wifi_provisioning/` | **WRAPPER** | Thin forwarding to wifi_manager (compat) |
| `modules/settings/` | Active | settings_t blob in NVS |
| `modules/theme/` | Active | Runtime color switching, `tconf` NVS |
| `modules/spectrum/` | Active | FFT visualizer, adjustable params |
| `modules/iching/` | Active | I Ching + Xiao Liu Ren |
| `modules/time_sync/` | Active | SNTP + NVS offset backup |
| `modules/power/` | Active | Battery ADC + charging status |
| `modules/retro_go/` | Partial | rg_adapter: in-process NES via retro-core (longjmp) |
| `modules/nes/` | **STUB** | Legacy, no longer compiled |
| `app/rom_browser.c` | **STUB** | Legacy NES browser (compiled but unused — NES now partition-switches to ota_0) |
| `components/nes_core/` | **DISABLED** | CMakeLists.txt.disabled |
| `components/retro-core/` | Active | nofrendo NES C source |

---

# Critical Gotchas

## Build Environment
- ESP-IDF refuses to run under MSYS2/Mingw. Set `$env:MSYSTEM=''` if launching from bash.
- Parallel builds on Windows intermittently fail (`cc1.exe: CreateProcess: No such file or directory`). Retry or limit with `ninja -j2`.
- If `idf.py build` fails with "component_manager: unknown command", ensure `IDF_COMPONENT_MANAGER` is set correctly.

## sdkconfig
- **Never delete `build/` without backing up `sdkconfig`** — `sdkconfig.defaults` alone doesn't restore LVGL fonts, partition scheme, or PSRAM config. `idf.py reconfigure` recreates from `sdkconfig.defaults` + `sdkconfig`.
- `idf.py set-target esp32s3` regenerates from `sdkconfig.defaults` which is incomplete — always restore `sdkconfig` from backup after target change.

## FreeRTOS Tick Rate: 1000Hz
`CONFIG_FREERTOS_HZ=1000` means **1ms tick** vs ESP-IDF default 10ms.
- `pdMS_TO_TICKS(1)` = 1 tick
- `vTaskDelay(1)` = ~1ms delay
- Timing loops from standard ESP-IDF projects run 10x faster here.

## LVGL Color Swap for ST7789
`CONFIG_LV_COLOR_16_SWAP=y` + `lv_conf.h` `LV_COLOR_16_SWAP 1`
- All RGB565 colors are byte-swapped before SPI (ST7789 requires MSB-first).
- **If adding raw pixel data manipulation**, byte-swap as well.

## Task WDT
`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n` and `CPU1=n`
- NES emulator starves idle_0 by design. InterTask WDT still active — blocking >5s on non-idle tasks triggers it.

## PSRAM Memory
- `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (default). **Never set `CONFIG_SPIRAM_USE_MALLOC=y`** — DMA components (mbedTLS, I2S, WiFi) crash.
- `managed_components/` is auto-generated — do not commit. `dependencies.lock` should be committed.

## Component Manager
- `IDF_COMPONENT_MANAGER=1` required for zlib (`espressif/zlib: ^1.3.1` in `main/idf_component.yml`). Only enable for weather builds.
- Weather API always returns gzip despite `&gzip=n`. Use `inflateInit2(&strm, 16 + MAX_WBITS)` for auto-detection.

## SPI Bus — 独立总线 (Rev 2 PCB)

Rev 2 PCB 不再共享 SPI 总线：LCD (SPI2_HOST) and SD 卡 (SPI3_HOST) each on separate buses. No bus contention.

## Retro-Go Display
- **Critical:** `RG_SCREEN_INIT()` must send `COLMOD=0x05` (MCU interface, 16-bit). `ili9341.h`'s `COLMOD=0x55` (RGB interface) does NOT work over SPI.
- **Display gap:** ST7789 has 240x320 memory, glass shows 240x280 (rows 20-299). `lcd_set_window()` adds `y_gap = (FULL_HEIGHT - HEIGHT)/2 = 20`.
- Target config at `components/retro-go/targets/esp-bsp-handheld/config.h`.

## CJK Font

Font at `components/lvgl/src/font/lv_font_simsun_16_cjk.c` (SimHei, not SimSun despite filename).

```bash
lv_font_conv --font C:/Windows/Fonts/simhei.ttf --size 16 --bpp 4 --format lvgl \
  -r 0x20-0x7F,0xA0-0xFF,0x2500-0x25FF,0x3000-0x303F,0xFF00-0xFFEF,0x2010-0x201F,0x2026 \
  --symbols <chinese_chars.txt> -o components/lvgl/src/font/lv_font_simsun_16_cjk.c --no-compress
```

- `chinese_chars.txt` is the symbol source (commit both if regenerating).
- Missing chars → □ boxes. Scan with `grep -rP '[\x{4e00}-\x{9fff}]' main/ --include="*.c"` and verify each against the symbol list.
- `lv_font_conv` installed globally via npm.

## GCC 14+ Compatibility
- Use `%zu` for `size_t` in printf. C++ files need `#include <cstring>`.
- `file(TO_CMAKE_PATH)` needs quotes around empty variables in CMake.

## .gitignore Policies

| Excluded | Reason |
|----------|--------|
| `build/` | Build artifacts |
| `managed_components/` | Auto-generated (keep `dependencies.lock` committed) |
| `sdkconfig` | Generated from `sdkconfig.defaults` + menuconfig |
| `.claude/` | Session data, memory |
| `*.txt` | Temp/log files |
| `xiaozhi-esp32-main/*`, `retro-go-esp32s3-st7789v-v1.1/`, `Rachel/` | Unused reference repos |

---

# Debugging Guide

## Memory Debugging

```c
// DMA memory (critical for WiFi/audio)
ESP_LOGI(TAG, "DMA free: %zu", heap_caps_get_free_size(MALLOC_CAP_DMA));
// PSRAM
ESP_LOGI(TAG, "PSRAM total: %zu, free: %zu",
    heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
    heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
// Full heap info
heap_caps_print_heap_info(MALLOC_CAP_8BIT);
// LVGL heap
lv_mem_monitor_t mm;
lv_mem_monitor(&mm);
printf("LVGL heap: used=%d, free=%d, frag=%d%%\n", mm.used_size, mm.free_size, mm.frag_pct);
```

## Hardware Diagnostics

- **I2C probe:** `i2c_master_probe(I2C_NUM_0, 0x38, 100)` — returns ESP_OK if device responds
- **I2C address map:** AHT20=0x38, LSM6DS3=0x6A (no audio codec I2C on Rev 2)
- **PSRAM:** `heap_caps_get_total_size(MALLOC_CAP_SPIRAM)` should return ~8MB. Returns 0 if PSRAM not initialized.
- **Battery:** Rev 2 has no ADC (IO3 used by AMP). Currently reports 100% to avoid low-battery warnings.

## Common Crashes

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `wifi:alloc eb fail` | WiFi out of DMA memory | Reduce LVGL buffer or increase PSRAM for WiFi |
| LVGL freeze after 3-5 switches | LVGL 64KB heap exhausted | Check for missing `lv_obj_del(old_scr)` |
| Crash on 2nd entry to screen | Dangled static pointer | Reset all static widget ptrs to NULL in create |
| `Guru Meditation Error` in game_task | Stack overflow (Core 1) | Increase game_task stack |
| cJSON_Parse returns NULL | No PSRAM for cJSON tree | `cjson_malloc` must force `MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT` |
| LCD horizontal stripes | Single buffer or power | Dual buffering + check VCOM/VRH in st7789_driver.c |

## Serial Log Tags

| Tag | Context |
|-----|---------|
| `MAIN` | Entry, key handler dispatch |
| `APP_MGR` | App lifecycle (launch/return) |
| `LAUNCHER` | Home screen update loop |
| `TIMER` | IMU readings + rotation status |
| `WEATHER` | Weather fetch, parse, cache |
| `NES` | Game lifecycle, frame timing |
| `WIFI` | Connection state, provisioning |
| `LVGL` | Display driver, buffer swaps |
| `RG_WRAP` | In-process retro-go rg_adapter |
| `BLE_HID` | BLE pairing, AUTH OK/ERROR |

## Weather Module Tips

- Default Location: `101230501` (泉州), Host: `devapi.qweather.com` (configured in `main/Kconfig.projbuild`)
- **API Key required** — configure via `idf.py menuconfig` → "ESP-Handheld Configuration" → "Weather API Key"
- Location configurable via Settings → 天气地区, stored in NVS `weather` namespace (keys `location_id`, `location_name`). 13 Quanzhou districts available.
- Location change triggers `xTaskNotifyGive()` to wake weather task for immediate refresh.
- API always returns gzip. cJSON allocators must force PSRAM with `MALLOC_CAP_8BIT`.

---

# Hardware Reference

## Pin Map (Rev 2 PCB)

```
LCD (SPI2_HOST):     BL=IO43 DC=IO44 CS=IO42 SCK=IO21 MOSI=IO14 RST=IO13
SD (SPI3_HOST):      CS=IO38 CLK=IO40 MOSI=IO39 MISO=IO41
                     (独立总线，不与LCD共享)
Digital Mic (I2S1 RX): BCLK=IO11 WS=IO12 DATA=IO10
MAX98357 (I2S0 TX):    BCLK=IO46 LRC=IO9 DIN=IO3
                     (无MCLK)
Sensor I2C:          SDA=IO1 SCL=IO2 (IMU + AHT20 共用)
Buttons:             UP=IO16 DOWN=IO15 LEFT=IO17 RIGHT=IO7
                     START=IO6 A=IO4 B=IO5
BOOT:                IO0
LED:                 IO18
Battery:             No ADC (IO3 used by AMP), no charging GPIO
```

## Code Conventions

- C99 for most code; C++ only in `modules/wifi_manager/wifi_bridge.cpp`
- `#include` paths use INCLUDE_DIRS from `main/CMakeLists.txt` (relative to `main/`)
- Settings stored in NVS, loaded via `settings_load(settings_t *s)`
- Inline comments bilingual (Chinese + English): Chinese for UI-facing, English for technical notes
- `#ifdef __cplusplus extern "C"` wrappers in shared headers

---

# Project Sources

| Component | Source |
|-----------|--------|
| Retro-Go game system | https://github.com/ducalex/retro-go |
| NES core (nofrendo) | Included in retro-go (`retro-core/components/nofrendo/`) |
| WiFi manager | https://github.com/78/esp-wifi-connect |
| LVGL v8.4 | https://github.com/lvgl/lvgl |
| XiaoZhi AI | https://github.com/78/xiaozhi-esp32 |
| Audio codec dev | Espressif component registry |
| Weather API | HeFeng (devapi.qweather.com) |
| NTP | ntp.aliyun.com + pool.ntp.org |
