# CLAUDE.md

Agent guidance for **ESP-Handheld** firmware — ESP32-S3 handheld with LVGL UI, NES emulator, weather, BLE HID, WiFi audio, and dual-firmware system.

**Project:** `esp_handheld` — ESP-IDF v5.5.4, target `esp32s3`, entry `main/main.c`

## Environment Setup

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| ESP-IDF | **v5.5.4** | Must target `esp32s3`. Do NOT use other versions — code relies on v5.5.4 APIs. |
| Python | 3.11+ | Bundled with ESP-IDF installer |
| CMake | ≥3.16 | Bundled with ESP-IDF |
| Ninja | ≥1.10 | Bundled with ESP-IDF |
| Git | Any | For cloning + submodules |

### Install ESP-IDF

**Windows:** Download the ESP-IDF v5.5.4 installer from [Espressif's website](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html). Select "ESP32S3" target during installation. After install, use the "ESP-IDF PowerShell" shortcut.

**Linux / macOS:**
```bash
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh     # add to .bashrc for convenience
```

**Verify installation:**
```bash
idf.py --version     # Should show v5.5.4
xtensa-esp32s3-elf-gcc --version  # Should show 14.2.0
```

## Build & Flash

### First Clone
```bash
# ⚠️ MUST use --recursive to get submodules (LVGL + esp-wifi-connect)
git clone --recursive https://github.com/zistar001/ESP-Handheld.git
cd ESP-Handheld

# If you forgot --recursive, run this:
git submodule update --init --recursive
```
Submodules: LVGL v8.4 (`components/lvgl`), esp-wifi-connect (`components/esp-wifi-connect`).
CJK font at `components/lvgl/src/font/lv_font_simsun_16_cjk.c` — already in repo.

### Build
```bash
# Must NOT be in MSYS2/Mingw — use PowerShell or cmd.exe
export IDF_COMPONENT_MANAGER=0     # 0 = offline (default), 1 = weather needs zlib
idf.py set-target esp32s3          # first time only (sets target + configures build)
idf.py menuconfig                  # optional: set Weather API Key
idf.py build
```

**Windows parallel build fix:** `idf.py build -- -j2` if `cc1.exe: CreateProcess` error.

### Flash
```bash
# Full (first time):
python -m esptool --chip esp32s3 -p (PORT) -b 921600 write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin

# App-only (updates):
python -m esptool --chip esp32s3 -p (PORT) -b 921600 write_flash 0x10000 build/esp_handheld.bin

# Monitor:
idf.py -p (PORT) monitor    # 115200 baud, Ctrl+] to exit
```

**CRITICAL:** Without `ota_data_initial.bin` at 0xd000, boots into wrong partition.

### API Key Configuration
```bash
idf.py menuconfig → "ESP-Handheld Configuration" → "Weather API Key"
```
Weather auto-disables if key is empty. Register at [devapi.qweather.com](https://devapi.qweather.com).

## Project Architecture

### Hardware (Reference PCB Rev 2)
ST7789 240×280 SPI LCD + SD card (independent SPI buses) + MAX98357 amp (I2S0 TX) + MSM261 digital mic (I2S1 RX) + LSM6DS3TR-C IMU + AHT20 temp/humidity (shared I2C) + 7 keys + 16MB flash + 8MB PSRAM.

**Pin map in** `main/bsp/bsp_board.h` — edit `BSP_*` defines to adapt to your PCB.

### Partition Layout (16MB)
| Partition | Offset | Size | Content |
|-----------|--------|------|---------|
| factory | 0x10000 | 4MB | ESP-Handheld (this firmware) |
| ota_0 | 0x410000 | 4MB | Retro-Go game system |
| ota_1 | 0x810000 | 4MB | XiaoZhi AI |
| assets | 0xC10000 | ~4MB | SPIFFS resources |

### Code Layout
```
main/
  main.c       — Entry: init sequence + key_handler() dispatch
  bsp/         — Board support (st7789, sd_card, bsp_board, key_driver)
  app/         — App framework (app_manager, launcher, menu)
  modules/     — audio/ imu/ sensor/ weather/ pc_remote/ wifi_manager/
                 settings/ power/ time_sync/ iching/ theme/ spectrum/
                 retro_go/   (rg_adapter: in-process NES via retro-core)
  ui/          — display_driver.c + screens/ + components/
components/
  lvgl/        — LVGL v8.4 (submodule)
  retro-core/  — nofrendo NES C source (in-process)
  retro-go/    — Retro-Go runtime (ota_0 standalone)
  esp-wifi-connect/ — WiFi provisioning (submodule)
```

### Init Sequence (main.c `app_main`)
NVS → Settings/Theme → SD Card → BSP Board (LCD) → LVGL → Audio → Sensors → Battery → BLE HID → Air Mouse → WiFi → Weather/Time → Key Driver → Power Mgmt → App Manager

### App State Machine
```
Launcher ←[START]→ Menu ←[B/START]→ Launcher
                    ↓ [A]        ↑
                 Running (app) ──┘ [B/START]
```

## Key Gotchas

| Issue | Cause | Fix |
|-------|-------|-----|
| MSYS2 detection | ESP-IDF refuses Mingw | Use PowerShell, or `$env:MSYSTEM=''` |
| 1000Hz tick | `CONFIG_FREERTOS_HZ=1000` = 1ms tick | Timing loops run 10x faster than default |
| RGB565 byte-swap | ST7789 needs MSB-first | `CONFIG_LV_COLOR_16_SWAP=y`, raw pixel data must swap |
| LVGL heap 64KB | Screen transitions leak | Always `lv_obj_del(old_scr)` + reset static pointers |
| PSRAM DMA crash | `SPIRAM_USE_MALLOC=y` | Keep `SPIRAM_USE_CAPS_ALLOC=y` |
| No test suite | No unit/integration tests | Validate via build → flash → monitor |
| Weather gzip | API ignores `&gzip=n` | `inflateInit2(&strm, 16 + MAX_WBITS)` for auto-detect |
| Retro-Go COLMOD | ST7789 needs MCU mode | `COLMOD=0x05` not `0x55` (RGB interface doesn't work over SPI) |

## Common Tasks

### Add a New App
1. `main/app/app_manager.h` — add `APP_ID_FOO` to enum
2. `main/app/app_manager.c` — add case in `app_manager_launch()`, call `foo_screen_create()`
3. `main/ui/screens/foo_screen.c/h` — create with `lv_obj_del(old_scr)` pattern
4. `main/ui/screens/menu_screen.c` — add entry to `items[]` array
5. `main/CMakeLists.txt` — add to `SRCS` + `INCLUDE_DIRS`
6. `main/main.c` — add `#include` + key handling in `key_handler()` → `APP_STATE_RUNNING`
7. Use `CLR_*` macros from `theme_colors.h` not hardcoded colors

### Adapt to Different Hardware
1. Edit `main/bsp/bsp_board.h` — GPIO pin definitions
2. Edit `main/bsp/st7789_driver.c` — display init sequence if different panel
3. Edit `main/modules/audio/box_audio_codec.c` — I2S config if different audio
4. If using external codec (ES8311 etc.), restore old codec drivers

### Debug
```c
ESP_LOGI(TAG, "DMA free: %zu", heap_caps_get_free_size(MALLOC_CAP_DMA));
heap_caps_print_heap_info(MALLOC_CAP_8BIT);
lv_mem_monitor_t mm; lv_mem_monitor(&mm);
```

Key log tags: `MAIN`, `APP_MGR`, `LAUNCHER`, `WEATHER`, `NES`, `WIFI`, `LVGL`, `RG_WRAP`, `BLE_HID`

### Build XiaoZhi AI (ota_1)

XiaoZhi is NOT included in this repo. Build it separately:

```bash
git clone https://github.com/78/xiaozhi-esp32.git
cd xiaozhi-esp32
# Create your board definition under boards/ (see docs)
idf.py set-target esp32s3
idf.py build
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x810000 build/xiaozhi.bin
```

The ESP-Handheld menu's "小智" card will reboot into ota_1 to launch XiaoZhi.

## NVS Quick Reference

| Namespace | Key | Content |
|-----------|-----|---------|
| `settings` | `cfg` (blob) | Volume/brightness/mic_gain/key_map |
| `tconf` | `cfg` (blob) | Theme config |
| `wifi` | SSID entries | Shared WiFi credentials |
| `weather` | `api_key`, `location_id`, `location_name` | Weather settings |
| `time` | `tz_offset` | Time sync offset |
| `voice` | `pc_octet` | WiFi audio PC IP |

## Third-Party Dependencies

| Component | Source | License | Relation |
|-----------|--------|---------|----------|
| nofrendo | retro-go repo | **GPL v2** | **Linked into main binary** — this is why the project must be GPL v2 |
| Retro-Go | https://github.com/ducalex/retro-go | GPL v2 | Standalone ota_0 firmware (separate binary) |
| LVGL v8.4 | https://github.com/lvgl/lvgl | MIT | Linked into main binary |
| esp-wifi-connect | https://github.com/78/esp-wifi-connect | No explicit license | Submodule (cloned from upstream) |
| XiaoZhi AI | https://github.com/78/xiaozhi-esp32 | MIT | Standalone ota_1 firmware (separate binary) |
| Weather API | HeFeng (devapi.qweather.com) | Free tier | External service |
| NTP | ntp.aliyun.com + pool.ntp.org | — | External service |
