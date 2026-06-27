# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Build & Flash

**IMPORTANT:** Never build from MSYS2/Mingw bash. ESP-IDF detects `MSYSTEM` and refuses. Use cmd.exe or PowerShell.

## Environment

Paths defined in `idf_env.ps1`. Before building, ask user which environment, then set paths accordingly.

| Environment | IDF_PATH | IDF_TOOLS_PATH | COM Port |
|-------------|----------|----------------|----------|
| 🏢 办公室 | `D:\esp\v5.5.4\esp-idf` | `C:\Espressif\tools` | COM3 |
| 🏠 家里 | `D:\Espressif\frameworks\esp-idf-v5.5.4` | `D:\Espressif\tools` | COM8 |

Common: target `esp32s3`, flash baud 921600.

## Canonical Build (PowerShell)

```powershell
$env:MSYSTEM=''
$env:IDF_PATH='D:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='0'   # set to '1' only for weather (zlib)
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

Retro-go is a standalone firmware flashed to the **ota_0** partition (4MB, offset 0x410000). It contains a ROM browser launcher + all emulator cores in a single binary.

### Build & Flash

```powershell
cd retro-go-firmware
powershell -ExecutionPolicy Bypass -File build_rg.ps1
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

Retro-go config at `components/retro-go/targets/esp-bsp-handheld/config.h`. Pin mappings match ESP_BSP.

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

## Flash XiaoZhi AI (zistar) to ota_1

```bash
# Build zistar board in xiaozhi-zistar-main first
cd xiaozhi-zistar-main
idf.py build
# Flash only the app binary to ota_1 partition
esptool.py -p COM7 -b 921600 write_flash 0x810000 build/xiaozhi.bin
```
XiaoZhi source at `xiaozhi-zistar-main/`, custom board: `boards/zistar/`. Must use the same partition layout as ESP_BSP (app linked at 0x810000).

## SPIFFS Assets

```bash
python $env:IDF_PATH/components/spiffs/spiffsgen.py 0x3F0000 assets/ build/assets.bin
python -m esptool --chip esp32s3 -p COM7 -b 921600 write_flash 0xC10000 build/assets.bin
```

## Monitor

```bash
idf.py -p COM7 monitor   # 115200 baud, Ctrl+] to exit
```

---

# Retro-Go Firmware Structure

```retro-go-firmware/
├── CMakeLists.txt         — Project def, refs D:\retro-go emulator cores + local retro-go component
├── main/
│   ├── main.c             — app_main routing + rg_system_exit override
│   ├── launcher_integration.c — ROM browser UI loop
│   └── CMakeLists.txt
├── deps/retro-core-mains/ — Emulator entry points (copied from D:\retro-go)
├── sdkconfig.defaults
└── build_rg.ps1
```

Only NES (nofrendo) emulator is currently in the main ESP_BSP `components/retro-core/`. Other emulators (gnuboy, smsplus, pce-go, handy, snes9x, gw-emulator) are referenced via `D:\retro-go\retro-core\components\` in the retro-go firmware CMakeLists.txt — they are NOT part of the ESP_BSP build and only run on ota_0.

---

# Project Architecture

ESP32-S3 handheld device firmware — LVGL UI + weather + BLE remote + WiFi audio + retro game launcher + dual-firmware with XiaoZhi AI. Built with **ESP-IDF v5.5.4**, project name `esp_handheld`. Entry point: `main/main.c`.

**Hardware (Rev 2 PCB):** ST7789 240×280 SPI LCD (SPI2_HOST), SD card on independent SPI3_HOST, MAX98357 digital amp (I2S0 TX), digital mic (I2S1 RX), LSM6DS3TR-C IMU + AHT20 temp/humidity (shared I2C), 7 physical keys, 16MB flash, 8MB Octal PSRAM.

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
    theme/            — Theme manager (6 themes, 5 color slots, NVS, mix-and-match)
    spectrum/         — Float32 FFT spectrum visualizer (mirrored bars, adjustable params)
    retro_go/         — rg_adapter.c: in-process retro-go emulator entry (NES only, longjmp-based)
    nes/              — STUB: legacy nes_wrapper, nes_game, font8x16 (no longer compiled)
  ui/
    display_driver.c  — LVGL init, double-buffer, lvgl_lock/lvgl_unlock
    screens/          — home, menu, settings, countdown, airmouse, kbd,
                        ip_input, fortune, liuren, calib, record, theme, spectrum
    components/       — status_bar, weather icon rendering, theme_colors macros
components/
  lvgl/               — LVGL v8.4 (local source, not from registry)
  nes_core/           — NES emulator C++ core (DISABLED — CMakeLists.txt.disabled)
  retro-core/         — nofrendo NES emulator C source (from retro-go, compiled in-process)
  retro-go/           — Retro-Go runtime: display/input/audio drivers + gamepad mapping
  esp-wifi-connect/   — WiFi provisioning (github.com/78/esp-wifi-connect)
  espressif__esp_codec_dev/ — Codec device abstraction
  lv_conf.h           — LVGL config (not Kconfig)
```

**Migration state:** NES games previously ran in-process via `components/nes_core/` (C++, CMakeLists.txt.disabled) and `main/modules/nes/` (wrapper stubs). They now run either as standalone firmware on ota_0 (retro-go, for all emulators) or in-process via `components/retro-core/` (nofrendo, C only) through `main/modules/retro_go/rg_adapter.c`. The old `modules/nes/` and `components/nes_core/` are dead code — kept for reference only.

## Partition Layout (16MB flash — Three Systems)

| Partition | Offset | Size | Content |
|-----------|--------|------|---------|
| factory   | 0x10000  | 4MB | ESP_BSP (LVGL launcher, weather, settings, utilities) |
| ota_0     | 0x410000 | 4MB | **Retro-Go (standalone game system)** |
| ota_1     | 0x810000 | 4MB | XiaoZhi AI firmware |
| assets    | 0xC10000 | ~4MB | SPIFFS resources |

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
| pm | 4096 | 1 | 0 | Power mgmt, Light Sleep check (5s) |
| launcher_upd | 4096 | 1 | 0 | Home screen 1s tick: update time, battery, WiFi, indoor temp/humidity |

Plus internal tasks created by subsystems: weather (periodic HTTP fetch), WiFi (event loop, provisioning), BLE (Bluedroid stack).

The retro-go firmware (ota_0) has its own task layout: video SPI transfer on Core 0, emulation on Core 1, audio on dedicated timer.

## App State Machine

```
Launcher ←──[START]──→ Menu ←──[B/START]──→ Launcher
                          ↓ [A select]
                       Running (app)
                          ↓ [B/START]
                       Menu
```

- **Launcher** = home screen (weather, time, status bar, indoor temp/humidity). A 1-second `launcher_upd` task refreshes time, battery, WiFi icon, and AHT20 readings. START → Menu.
- **Menu** = 2-column scrollable grid, 9 app cards (游戏/小智/键盘/鼠标/占卜/小六壬/计时/频谱/设置). Golden highlight border (`CLR_HIGHLIGHT`, default `0xFFBB00`) + 7% fill tint. Navigation stops at edges. Remembers last selection across visits via static `sel` variable.
- **"游戏" card** = partition-switches to ota_0 (Retro-Go).
- **"小智" card** = partition-switches to ota_1 (XiaoZhi). Toggles: factory/ota_0→ota_1, ota_1→factory.
- **Running** = the active app. B or START returns to Menu (most apps).
- **`app_manager_set_return_to(APP_ID_SETTINGS)`** makes the next return go to settings instead of menu (used by WiFi reset flow).

## Key Handler Dispatch

`key_handler()` in `main.c` is called from the key task (Core 0, prio 2). It's a central switch on `app_manager_get_state()`:

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
    APP_ID_XIAOZHI:   partition-switch to ota_1 (XiaoZhi) / toggle back to factory
    APP_ID_KEYBOARD:  physical keys → HID codes, START+B=exit, A=Enter, RIGHT=PTT voice
    APP_ID_MOUSE:     A=left click, B=right click, UP/DOWN=sensitivity, START+B=exit
    APP_ID_SETTINGS:  D-pad nav in list/sub-screens, A=select, B/START=back/exit
    APP_ID_FORTUNE:   D-pad nav, A=select divination type
    APP_ID_RECORDER:  D-pad nav, A=trigger Xiao Liu Ren calculation
    APP_ID_COUNTDOWN: START+B combo=exit, A=reset when finished
    APP_ID_CALIB:     A=trigger IMU calibration, B/START=exit
    APP_ID_THEME:     UP/DOWN=nav, A=select/enter, B/START=exit
    APP_ID_SPECTRUM:  UP/DOWN=select param, LEFT/RIGHT/A=adjust, B/START=exit
    APP_ID_IP_INPUT:  UP/DOWN=nav, A=select, B/START=cancel+exit
    APP_ID_WIFI_SETUP:A=reconfigure (deinits BLE, starts WiFi AP), B/START=exit
    default:          B/START → return to menu (catch-all)
```

**Pattern for adding a new app:** Add an `else if` block in `APP_STATE_RUNNING` checking `app_manager_get_current_app()`.

## Init Sequence (`app_main` in `main.c`)

The boot order matters — SD card must init first to claim SPI bus at safe speed:

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

## Sleep Prevention

The power management task (`pm_task`, 5s interval) skips sleep when:
- `APP_ID_COUNTDOWN` is active (timer must stay visible)
- `APP_ID_SPECTRUM` is active (spectrum must stay visible)

---

# Screen System

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

### Thread Safety

LVGL calls from **outside the LVGL timer handler** must hold the mutex:
```c
lvgl_lock();
lv_obj_set_style_bg_color(some_btn, lv_color_hex(0xFF0000), 0);
lvgl_unlock();
```

- `key_handler` calls always need locking (runs on Core 0, prio 2)
- Sensor/IMU task callbacks updating labels need locking
- `launcher_upd` task calls need locking (updates home screen every second)

### Home Screen Cache

Home screen restores its state from static cache variables (`s_cached_*` in `home_screen.c`). The big weather icon is restored from `s_cached_desc` — it's NOT auto-updated from forecast data. Any new widget added to the home screen must be cached the same way.

## Settings SubScreen Architecture

The settings screen uses an integer `s_sub_mode` to track which sub-screen is active:
- `s_sub_mode == 0`: main settings list (volume, brightness, WiFi, weather location, theme, sleep, about, etc.)
- `s_sub_mode > 0`: a specific sub-screen (e.g., volume slider, location picker, key remapping)
- Each sub-screen replaces the list content with its own widgets, handled by render functions within `settings_screen.c`
- Navigation: `settings_screen_navigate(delta)` moves selection; `settings_screen_select()` enters sub-screen or toggles value; `settings_screen_back()` returns from sub-screen to main list (returns `true` if handled, `false` if already at main list → caller should exit settings)

Key pattern: `settings_screen_create()` is called with a `settings_t*` and can be re-called to re-render after changes.

## Screen File Reference

| Screen | File(s) | App IDs | Key Behavior |
|--------|---------|---------|--------------|
| Home | `home_screen.c/.h` | Launcher state | Auto-refreshed by launcher_upd task |
| Menu | `menu_screen.c/.h`, `menu.h/.c` | Menu state | 9 cards, 2-column grid, scrollable |
| Settings | `settings_screen.c/.h` | `APP_ID_SETTINGS` | Sub-screen architecture, NVS-backed |
| Keyboard | `kbd_screen.c/.h` | `APP_ID_KEYBOARD` | HID key codes + PTT voice |
| Air Mouse | `airmouse_screen.c/.h` | `APP_ID_MOUSE` | Toggle mouse mode + sensitivity |
| Countdown | `countdown_screen.c/.h` | `APP_ID_COUNTDOWN` | START+B exit, A reset |
| Fortune | `fortune_screen.c/.h` | `APP_ID_FORTUNE` | I Ching hexagram selection |
| Liu Ren | `liuren_screen.c/.h` | `APP_ID_RECORDER` | Xiao Liu Ren divination |
| Calibration | `calib_screen.c/.h` | `APP_ID_CALIB` | IMU calibration trigger |
| Theme | `theme_screen.c/.h` | `APP_ID_THEME` | Theme selection + slot override |
| Spectrum | `spectrum_screen.c/.h` | `APP_ID_SPECTRUM` | Adjustable FFT params |
| IP Input | `ip_input.c/.h` | `APP_ID_IP_INPUT` | PC IP address entry |
| Record | `record_screen.c/.h` | (not in menu) | WiFi audio voice recorder toggle screen, compiled but unregistered |
| WiFi Setup | Built into `app_manager.c` | `APP_ID_WIFI_SETUP` | Inline LVGL screen, not in menu |
| About | Built into `app_manager.c` | `APP_ID_ABOUT` | System info display, not in menu |

---

# Theme System

Theme system provides runtime color switching via NVS-persisted config (`tconf` namespace). All UI screens support it via `CLR_*` macros.

## Color Slots (5)

| Slot | Usage | Example |
|------|-------|---------|
| `TSLOT_BG` | Background color | `0x0A0A0A` |
| `TSLOT_ACCENT` | Active buttons, primary highlight | `0xFF5C00` |
| `TSLOT_HIGHLIGHT` | Menu selection border, bright accent | `0xFFBB00` |
| `TSLOT_TEXT` | Primary text | `0xFFFFFF` |
| `TSLOT_SUBTEXT` | Secondary text, hints | `0x999999` |

## 6 Built-in Themes

```
                    BG         ACCENT     HIGHLIGHT  TEXT      SUBTEXT
经典橙 (default)    #0A0A0A   #FF5C00    #FFBB00    #FFFFFF  #999999
极光蓝             #0D1117   #0096FF    #58A6FF    #FFFFFF  #8B949E
赛博紫             #0A0015   #BB86FC    #00FFFF    #FFFFFF  #8870A0
翡翠绿             #0A0F0A   #00C853    #69F0AE    #FFFFFF  #A5D6A7
暗紫金             #120A1A   #BB86FC    #FFD700    #FFFFFF  #9980B0
钢铁灰             #121212   #FF6D00    #FF9100    #FFFFFF  #B0B0B0
```

## Using Theme Colors

In any screen .c file, `#include "ui/components/theme_colors.h"` then use `CLR_BG`, `CLR_ACCENT`, `CLR_HIGHLIGHT`, `CLR_TEXT`, `CLR_SUBTEXT` macros instead of hardcoded `lv_color_hex(...)`. These call `theme_get_color()` at runtime so screens update immediately on theme change.

## Adding a New Theme

Add entry in `s_themes[]` in `modules/theme/theme_manager.c` and enum value in `theme_id_t` in the header.

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
| `rg` | various | Retro-Go settings (boot config on SD card, not NVS) |

## `settings_t` Struct

See `modules/settings/settings_manager.h`:

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

## ESP_BSP ↔ XiaoZhi
Menu → 小智 card → detects current partition and toggles: factory/ota_0→ota_1, ota_1→factory. Calls `esp_ota_set_boot_partition()` + `esp_restart()`.

## XiaoZhi → ESP_BSP
**START+B** (GPIO15+GPIO16) held for ~150ms → `ReturnToHandheld()` → reboots to factory.

## XiaoZhi Side Button Map

| Key | GPIO | XiaoZhi Function |
|-----|------|-----------------|
| **A** | 17 | Push-to-Talk (hold to speak) |
| **UP** | 5 | Volume + |
| **DOWN** | 6 | Volume - |
| **START** | 15 | Combo: START+B = return to ESP_BSP |
| **B** | 16 | Same combo |
| LEFT/RIGHT | 4,7 | Unused |

## WiFi Sharing

Both ESP_BSP and XiaoZhi use the same `esp-wifi-connect` SsidManager with NVS namespace `"wifi"`. Credentials configured in either system are available to the other automatically.

---

# How to Add a New App

1. **app_manager.h** — Add `APP_ID_FOO` to the `app_id_t` enum
2. **app_manager.c** — Add case in `app_manager_launch()` switch, call `foo_screen_create()`
3. **ui/screens/foo_screen.c/h** — Create screen with create/destroy, handle input
4. **ui/screens/menu_screen.c** — Add entry to `items[]` array (icon + label + app_id); `item_count` must match
5. **main/CMakeLists.txt** — Add source files to `SRCS` and include dirs to `INCLUDE_DIRS`
6. **main.c** — Add `#include "ui/screens/foo_screen.h"` and key handling in `key_handler()` under `APP_STATE_RUNNING`
7. **Theme support** — Use `CLR_*` macros from `theme_colors.h` instead of hardcoded colors

---

# Module Status Map

| Module | Status | Notes |
|--------|--------|-------|
| `modules/audio/` | Active | MAX98357 + 数字麦克风, I2S0 TX + I2S1 RX (独立) |
| `modules/audio/es8311_driver.*` | **REMOVED** | Rev 1遗留，不再编译 |
| `modules/audio/es7210_driver.*` | **REMOVED** | Rev 1遗留，不再编译 |
| `modules/imu/` | Active | LSM6DS3TR-C, on-demand (no dedicated task) |
| `modules/sensor/` | Active | AHT20, polled from launcher_upd task |
| `modules/weather/` | Active | HeFeng API, background task, NVS location |
| `modules/pc_remote/` | Active | BLE HID, air mouse (gyro→mouse), WiFi audio |
| `modules/wifi_manager/` | Active | esp-wifi-connect C++ bridge |
| `modules/settings/` | Active | settings_t blob in NVS |
| `modules/theme/` | Active | Runtime color switching, `tconf` NVS |
| `modules/spectrum/` | Active | FFT visualizer, adjustable params |
| `modules/iching/` | Active | I Ching + Xiao Liu Ren |
| `modules/time_sync/` | Active | SNTP + NVS offset backup |
| `modules/power/` | Active | Battery ADC + charging status |
| `modules/retro_go/` | Partial | rg_adapter: in-process NES via retro-core (longjmp-based) |
| `modules/nes/` | **STUB** | Legacy wrappers for old C++ core, no longer compiled |
| `app/rom_browser.c` | **STUB** | Legacy ROM browser for old in-process NES, still compiled (`rom_browser_key()` called from key_handler NES case) but NES now partition-switches to ota_0 |
| `components/nes_core/` | **DISABLED** | CMakeLists.txt.disabled, old C++ emucore |
| `components/retro-core/` | Active | nofrendo NES (C source, used by rg_adapter) |

---

# Audio Subsystem (Rev 2 PCB)

两个独立I2S控制器，各自独立时钟。无外部音频编解码芯片。

- **I2S_NUM_0 (RX):** BCLK=IO11, WS=IO12, DATA=IO10 → MSM261 数字麦克风
- **I2S_NUM_1 (TX):** BCLK=IO46, LRC=IO9, DIN=IO3 → MAX98357 数字功放
- 16kHz, 16bit. 音量/增益由软件控制（乘法器）。
- Volume/mic gain NVS-persisted in `settings_t`.

ESP_BSP的 `box_audio_codec.c` 使用独立I2S通道。XiaoZhi 使用 `NoAudioCodecSimplex` 类（见 `no_audio_codec.cc`）。

旧版 (Rev 1) 参考：旧PCB使用 ES8311 DAC + ES7210 ADC 通过共享 I2C 控制，I2S_NUM_0 同时做 TX (STD) + RX (TDM 4槽)。这些模块已不再编译: `es8311_driver.c`, `es7210_driver.c`。

### 麦克风增益

MSM261 数字麦克风输出信号小，需要软件放大：

```c
// 增益映射：0-37dB → 1.0x-70x
if (db >= 37) gain = 70.0f;
else if (db >= 30) gain = 35.0f;
else if (db >= 20) gain = 15.0f;
else if (db >= 10) gain = 5.0f;
else gain = 1.0f + db * 0.5f;
```

`box_audio_read()` 从 I2S 立体声读取左右两个声道，取**信号强的那个**（适配不同 MSM261 变种可能输出在 LEFT 或 RIGHT）。

### 功放上电时序

MAX98357 在 BCLK 开始后需要 ~12ms 稳定时间。`init_i2s1_tx()` 在 `i2s_channel_enable()` 后加了 25ms 延迟。

### 诊断

启动时会调用 `box_audio_diag()` 打印 I2S RX 数据统计（看 `BOX_AUDIO` 标签）：
- `ret=ESP_OK` + `range` 非零 → 麦克风正常
- `range=[0..0]` → 麦克风无数据，可能 PDM 变种需要改 I2S 模式

## WiFi Audio Streaming Protocol

See `modules/pc_remote/wifi_audio.c` + `tools/pc_voice_receiver.py`.

- **Audio data:** UDP unicast to configured PC IP, port **9210**
- **Sample format:** 16kHz, 16-bit signed PCM, 256 samples/packet (16ms audio)
- **Task:** `audio_tx_task` on Core 1, prio 3, stack 4096
- **PTT control:** UDP port **9212**, sends `"start"` or `"stop"` as control messages
- **PC IP configuration:** Set the last octet via IP Input screen; full IP is rebuilt from WiFi subnet (e.g., ESP at 192.168.1.100 + last octet 50 → 192.168.1.50)
- **NVS persistence:** Last octet saved in `voice` namespace key `pc_octet`
- **Receiver:** `tools/pc_voice_receiver.py` on the PC side

## BLE HID

Bluedroid BLE, HID keyboard + mouse. Just Works pairing, device name "ESP-Handheld". Passkey hardcoded "1234". API: `ble_hid_send_key(modifier, key)`, `ble_hid_send_mouse(buttons, dx, dy)`.

## WiFi Manager

Uses `78/esp-wifi-connect` v3.1.4 (C++, bridged to C via `wifi_bridge.cpp`). Soft-AP + DNS hijack + built-in web config page. Multi-SSID storage via SsidManager. Auto-migrates old settings_manager WiFi data.

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

## sdkconfig Critical Configs

These non-default settings (set in `sdkconfig.defaults`) affect all timing and rendering code:

### FreeRTOS Tick Rate: 1000Hz (was 100Hz)
`CONFIG_FREERTOS_HZ=1000` means **1ms tick** vs ESP-IDF default 10ms.
- `pdMS_TO_TICKS(1)` = 1 tick (not 1/portTICK_PERIOD_MS=1 on default 100Hz)
- `vTaskDelay(1)` = ~1ms delay
- **Watch out when porting code from standard ESP-IDF projects** — timing loops that assume 10ms tick will run 10x faster.
- Why: precise frame timing for display updates and audio streaming.

### LVGL Color Swap for ST7789
`CONFIG_LV_COLOR_16_SWAP=y` + `lv_conf.h` `LV_COLOR_16_SWAP 1`
- All RGB565 colors are byte-swapped before sending to SPI (ST7789 requires MSB-first).
- `lv_color_hex(0xFF0000)` produces different raw bytes than on a standard LVGL build.
- **If adding raw pixel data manipulation**, you must byte-swap as well.

### Task WDT
`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n` and `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n`
- Idle task watchdog is disabled on both cores. The retro-go NES emulator starves idle_0 by design.
- InterTask WDT is still active — long blocking operations (>5s default) on non-idle tasks will still trigger.

## PSRAM Memory
- `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` (default). **Never set `CONFIG_SPIRAM_USE_MALLOC=y`** — DMA components (mbedTLS, I2S, WiFi) crash.
- LVGL 双缓冲推荐使用 PSRAM (DMA 内存不足时的降级方案)。
- `managed_components/` is auto-generated — do not commit. `dependencies.lock` should be committed.

## Component Manager
- `IDF_COMPONENT_MANAGER=1` required for zlib (`espressif/zlib: ^1.3.1` in `main/idf_component.yml`). ESP_BSP normally has it disabled — only enable for weather builds.
- The zlib dependency is needed by weather module (HeFeng API always returns gzip despite `&gzip=n`). Use `inflateInit2(&strm, 16 + MAX_WBITS)` for auto-detection.

## SPI Bus — 独立总线 (Rev 2 PCB)

Rev 2 PCB 不再共享 SPI 总线：
- **LCD (ST7789):** SPI2_HOST (SCK=IO21, MOSI=IO14, CS=IO42, DC=IO44)
- **SD 卡:** SPI3_HOST (CLK=IO40, MOSI=IO39, MISO=IO41, CS=IO38)
- 两条总线各自独立，互不影响。SD 卡不再需要与 LCD 协调时序。

## Retro-Go Firmware Notes

### Memory
- Frame buffers and surfaces use `MEM_SLOW` (PSRAM) to avoid exhausting internal SRAM.
- `app_main()` calls `heap_caps_malloc_extmem_enable(1024)` so all `malloc()` calls from emulator cores go to PSRAM.
- Retro-go's DMA buffers for SPI display are allocated from `MEM_DMA` (internal SRAM) — keep these small.
- Boot config (`boot.json`) stored on SD card at `/sdcard/retro-go/config/`.

### Boot Config Reliability
- Boot config (which emulator + ROM to launch) is saved to SD card via `rg_settings`.
- `rg_system_exit()` (game menu "Exit"/"Save+Exit"): clears boot config via
  `unlink(boot.json)` + `rg_settings` API, then restarts same partition (ota_0) → boot reads empty config → launcher.
- `START+B` combo (in `rg_input.c`): switches to factory (ESP_BSP) by calling `esp_ota_set_boot_partition(factory)`.
- `rg_system.c`'s `rg_system_exit` was modified to NOT call `rg_system_switch_app("launcher")` since no "launcher" partition exists.
- A 1.5s delay ensures SD card writes complete before `esp_restart()`.

### Display
- ST7789 240x280, driver 0 (ILI9341/ST7789 direct SPI).
- **Critical:** `RG_SCREEN_INIT()` must send `COLMOD=0x05` (MCU interface, 16-bit). ili9341.h sends `COLMOD=0x55` (RGB interface) which does NOT work over SPI (display ignores all subsequent SPI commands → black screen). The SWRESET in `RG_SCREEN_INIT` clears the wrong value so 0x05 takes effect.
- Full-memory (240x320) clear NOT possible: SPI CS toggles between transactions, breaking ST7789's continuous RAM write mode. Gap at edges handled by caller.
- Target config at `components/retro-go/targets/esp-bsp-handheld/config.h`.

## LVGL Memory
- LVGL heap: 64KB (`LV_MEM_SIZE`). After ~3-5 screen transitions without deleting old screens, allocation fails → freeze.
- Home screen data: cached in static variables (`home_screen.c`). On navigation return, `home_screen_create()` restores from cache.
- All LVGL calls from tasks (key_handler, launcher_upd, sensor, etc.) must use `lvgl_lock()/lvgl_unlock()`.
- The weather icons font file (`weather_icons_font.c`) is NOT compiled in CMakeLists.txt — icons use LVGL symbols instead. If adding font-based weather icons, add to SRCS.

## GCC 14+ Compatibility
- Use `%zu` for `size_t` in printf. C++ files need explicit `#include <cstring>`.
- `file(TO_CMAKE_PATH)` needs quotes around empty variables.
- Add new modules to both `SRCS` and `REQUIRES`/`INCLUDE_DIRS` in `main/CMakeLists.txt`.

## Hardware Pin Map (Rev 2 PCB)

```
LCD (SPI2_HOST):    BL=IO43 DC=IO44 CS=IO42 SCK=IO21 MOSI=IO14 RST=IO13
SD (SPI3_HOST):     CS=IO38 CLK=IO40 MOSI=IO39 MISO=IO41
                    (独立总线，不再与LCD共享)
数字麦克风 (I2S1 RX): BCLK=IO11 WS=IO12 DATA=IO10
数字功放 MAX98357 (I2S0 TX): BCLK=IO46 LRC=IO9 DIN=IO3
                    (无MCLK，无外部编解码芯片)
传感器 I2C:          SDA=IO1 SCL=IO2  (IMU + AHT20 共用)
按键:               UP=IO16 DOWN=IO15 LEFT=IO17 RIGHT=IO7
                    START=IO6 A=IO4 B=IO5
BOOT:               IO0
LED:                IO18
电池:               无ADC检测 (IO3被AMP占用)，无充电状态GPIO
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
- **I2C address map (Rev 2):** AHT20=0x38, LSM6DS3=0x6A (已无音频编解码器I2C)
- **PSRAM verification:** `heap_caps_get_total_size(MALLOC_CAP_SPIRAM)` should return ~8MB. Returns 0 if PSRAM is not initialized.
- **电池:** Rev 2 PCB 不通过ADC检测电池电压（IO3被MAX98357占用）。
  电池状态 GPIO 需用户自定义。目前始终报告 100% 电量以避免低电量警告。

## Common Crashes

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `wifi:alloc eb fail` | WiFi driver out of DMA memory | Reduce LVGL buffer size or increase PSRAM allocation for WiFi |
| LVGL freeze after 3-5 screen switches | LVGL 64KB heap exhausted from undeleted screens | Check for missing `lv_obj_del(old_scr)` in screen create functions |
| Crash on 2nd entry to a screen | Dangling static pointer from previous `lv_obj_del` | Reset all static widget pointers to NULL in `*_screen_create()` |
| `Guru Meditation Error` in game_task | Stack overflow on Core 1 (game_task) | Increase game_task stack or reduce stack usage in ROM loading |
| cJSON_Parse returns NULL in weather | No PSRAM available for cJSON tree | Check `cjson_malloc` hooks force `MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT` |
| LCD horizontal stripes | 单缓冲或电源不稳定 | 需双缓冲 + 检查 VCOM/VRH 电压设置 (st7789_driver.c 有补充初始化) |

## Serial Log Tags

| Tag | Context |
|-----|---------|
| `MAIN` | Entry, key handler dispatch |
| `APP_MGR` | App lifecycle (launch/return) |
| `LAUNCHER` | Home screen update loop |
| `TIMER` | IMU readings (`ax= ay= az=`) + rotation status |
| `WEATHER` | Weather fetch, parse, cache |
| `NES` | Game lifecycle, frame timing |
| `WIFI` | Connection state, provisioning |
| `LVGL` | Display driver, buffer swaps |
| `RG_WRAP` | In-process retro-go rg_adapter |

## Weather Module Tips

- HeFeng API key: `700cf8ab08774bf089e52d33b89aecf8`, Default Location: `101230501` (泉州), Host: `p23p3qvugk.re.qweatherapi.com`
- **Location is user-configurable** via Settings → 天气地区. Stored in NVS namespace `"weather"` (keys `location_id`, `location_name`). 13 Quanzhou districts available, some share the same API ID but display name is preserved.
- Location change triggers `xTaskNotifyGive()` to wake weather task for immediate refresh.
- API always returns gzip (ignores `&gzip=n`). Must use `inflateInit2(&strm, 16 + MAX_WBITS)`.
- cJSON allocators must force PSRAM with `MALLOC_CAP_8BIT` or they return NULL silently.
- Weather data processes regardless of current screen. Home screen restores from `s_cached_*` static vars.

---

# Code Conventions

- C99 for most code; C++ only in `modules/wifi_manager/wifi_bridge.cpp` (esp-wifi-connect bridge).
- `#include` paths use INCLUDE_DIRS from `main/CMakeLists.txt` (relative to `main/`).
- Settings stored in NVS (`settings` namespace), loaded via `settings_load(settings_t *s)`.
- Inline comments are bilingual (Chinese + English). Chinese for UI-facing descriptions, English for technical notes.
- `#ifdef __cplusplus extern "C"` wrappers used in headers that may be included by both C and C++.

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
