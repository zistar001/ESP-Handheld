# AGENTS.md

Guide for AI coding agents (Cursor, Copilot, Claude Code, etc.) working with this repository.

## Project Overview

ESP-Handheld is an **ESP32-S3** firmware built with **ESP-IDF v5.5.4**. It runs a LVGL graphical UI on a 240×280 ST7789 display, with NES emulation, weather dashboard, BLE HID keyboard/mouse, WiFi audio streaming, and a dual-firmware partition system.

**Entry point:** `main/main.c` — `app_main()` function.
**Build system:** ESP-IDF (CMake + Ninja).

## What the User Needs From You

When someone clones this repo and asks for help, they typically need one of:

1. **"Build it"** — Set up environment, configure, compile, flash
2. **"Adapt it to my hardware"** — Change GPIO pins for their PCB
3. **"Add a feature"** — New app, new screen, new module
4. **"Fix a bug"** — Debug crashes, display issues, WiFi/BLE problems
5. **"Configure API keys"** — Set up weather service

---

## 1. Build & Onboard the User

### Environment Requirements

When helping the user set up, use this table to answer "what version do I need":

| Tool | Required Version | Why |
|------|-----------------|-----|
| **ESP-IDF** | **v5.5.4** (exactly) | Code depends on v5.5.4 APIs. Other versions may fail to compile. |
| **Target** | `esp32s3` | The only supported MCU. Do NOT use other targets. |
| **Python** | 3.11+ | Bundled with ESP-IDF installer |
| **CMake** | ≥3.16 | Bundled with ESP-IDF |
| **Ninja** | ≥1.10 | Bundled with ESP-IDF |
| **Toolchain** | xtensa-esp-elf 14.2.0 | Automatically installed by ESP-IDF install script |

### Prerequisites Checklist

- [ ] ESP-IDF v5.5.4 installed with `esp32s3` target
- [ ] Git installed
- [ ] Python 3.11+ (bundled with ESP-IDF)
- [ ] CMake + Ninja (bundled with ESP-IDF)

### Install ESP-IDF (Guide the User)

**Windows users:**
1. Go to [Espressif ESP-IDF Windows Installer](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html)
2. Download the **ESP-IDF v5.5.4** offline or online installer
3. Run the installer, select **ESP32S3** target
4. After installation, search for "ESP-IDF PowerShell" and launch it
5. Verify: `idf.py --version` should show v5.5.4

**Linux / macOS users:**
```bash
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
echo ". $(pwd)/export.sh" >> ~/.bashrc   # or ~/.zshrc
. ~/.bashrc
idf.py --version   # Verify: should show v5.5.4
```

### Step-by-Step (tell the user)

**⚠️ Critical: You MUST clone with `--recursive`** to get the LVGL and esp-wifi-connect submodules.

```bash
# 1. Clone with submodules (MUST use --recursive)
git clone --recursive https://github.com/zistar001/ESP-Handheld.git
cd ESP-Handheld

# If you forgot --recursive, run this:
git submodule update --init --recursive
```

```bash
# 2. Set up ESP-IDF environment
# Windows: launch "ESP-IDF PowerShell" shortcut
# Linux/macOS:
. $IDF_PATH/export.sh

# 3. First-time only: set target
idf.py set-target esp32s3

# 4. Configure API keys (optional)
idf.py menuconfig
# → "ESP-Handheld Configuration" → Weather API Key

# 5. Build
export IDF_COMPONENT_MANAGER=0   # 0 = offline (default); 1 = weather needs zlib
idf.py build
```

**⚠️ Environment variable note:** If you see `ESP_ROM_ELF_DIR environment variable is not defined`, it's harmless — the firmware builds fine. To silence it, run the ESP-IDF export/install scripts properly.

**⚠️ Windows parallel build fix:** If `cc1.exe: CreateProcess` error occurs, retry with:
```bash
idf.py build -- -j2
```

# 5. Flash (full, first time)
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin

# 6. Monitor
idf.py -p (PORT) monitor
```

**CRITICAL notes to tell the user:**
- Never build from MSYS2/Mingw — ESP-IDF refuses. Use PowerShell or cmd.exe.
- The `ota_data_initial.bin` at 0xd000 is required for first flash, otherwise it boots the wrong partition.
- `IDF_COMPONENT_MANAGER=0` = offline build. Set to `1` if weather (zlib) is needed.
- If parallel build fails on Windows, retry with `idf.py build -j2`.

### Windows-Specific
```powershell
# If running from PowerShell launched from bash:
$env:MSYSTEM=''
# Use the canonical build method:
$env:IDF_COMPONENT_MANAGER='0'
idf.py build
```

---

## 2. Hardware Adaptation

If the user has a different PCB, guide them through editing these files:

### `main/bsp/bsp_board.h` — GPIO Pin Definitions
```c
#define BSP_LCD_BL      GPIO_NUM_43   // Change these
#define BSP_LCD_DC      GPIO_NUM_44   // to match the
#define BSP_LCD_CS      GPIO_NUM_42   // user's PCB
// ... etc for all BSP_* defines
```

### When the display is different:
- **Different resolution:** Update `BSP_LCD_WIDTH`/`HEIGHT` in `bsp_board.h`, and LVGL buffer size in `ui/display_driver.c`
- **Different controller (not ST7789):** Replace `main/bsp/st7789_driver.c` with the appropriate driver
- **Shared SPI bus:** The Rev 2 PCB uses separate SPI2_HOST (LCD) + SPI3_HOST (SD). If sharing a bus, you'll need bus arbitration

### When audio is different:
- **Different I2S config:** Edit `main/modules/audio/box_audio_codec.c`
- **External codec (ES8311/ES7210):** The old drivers are removed from compilation but the code may still exist in git history as reference

### Common hardware adaptation tasks the agent can help with:

| User says | What to do |
|-----------|-----------|
| "My screen is 320x240" | Update `bsp_board.h` dimensions, check LVGL buffer, update display driver init sequence |
| "My buttons are on different GPIOs" | Edit `BSP_KEY_*` defines in `bsp_board.h` |
| "I don't have IMU" | IMU init is best-effort — just leave unconnected, it won't block boot |
| "I have a different mic/amp" | Edit I2S pin configs in `bsp_board.h` and I2S initialization in `box_audio_codec.c` |
| "I want to use internal ADC for battery" | Rev 2 has no ADC for battery. Add ADC init in `modules/power/battery_monitor.c` and wire the GPIO |

---

## 3. Configuration System (Kconfig)

The project uses a Kconfig-based configuration system defined in `main/Kconfig.projbuild`:

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_WEATHER_API_KEY` | `""` | HeFeng weather API key. Empty = weather disabled. |
| `CONFIG_WEATHER_DEFAULT_LOCATION` | `"101230501"` | Default weather location ID |
| `CONFIG_BLE_DEVICE_NAME` | `"ESP-Handheld"` | Bluetooth HID device name |
| `CONFIG_WIFI_AP_SSID_PREFIX` | `"ESP-Handheld"` | WiFi AP SSID during config mode |

**To configure:** `idf.py menuconfig` → "ESP-Handheld Configuration"

**How the agent should handle API keys:**
1. Tell user to register at the service website
2. Guide them to configure via menuconfig
3. Explain that keys are stored in NVS on first boot, not in source code
4. If weather is not configured, it auto-disables gracefully

---

## 4. Common Modifications

### Add a New App (Screen)

The user says "I want to add a new screen/app". Steps:

1. **`main/app/app_manager.h`** — Add `APP_ID_FOO` before `APP_ID_MAX`
2. **`main/app/app_manager.c`** — Add `case APP_ID_FOO: foo_screen_create(); break;` in `app_manager_launch()`
3. **`main/ui/screens/foo_screen.c`** — Create with this pattern:
   ```c
   #include "ui/components/theme_colors.h"

   void foo_screen_create(void) {
       lv_obj_t *old = lv_scr_act();
       lv_obj_t *scr = lv_obj_create(NULL);
       // Reset ALL static widget pointers to NULL here
       // Create widgets using CLR_* macros for colors
       lv_scr_load(scr);
       lv_obj_del(old);
   }
   ```
4. **`main/ui/screens/menu_screen.c`** — Add to `items[]` array, increment `item_count`
5. **`main/CMakeLists.txt`** — Add to `SRCS`
6. **`main/main.c`** — Add `#include` and key handling in `key_handler()` under `APP_STATE_RUNNING`

**LVGL Gotchas (must tell the user):**
- LVGL heap is only **64KB** — always delete old screen with `lv_obj_del(old_scr)`
- Static widget pointers **dangle** after screen delete — reset to NULL on create
- All LVGL calls from tasks must use `lvgl_lock()` / `lvgl_unlock()`

### Theme Colors

All screens should use these macros instead of hardcoded colors:
```c
#include "ui/components/theme_colors.h"
CLR_BG        // Background
CLR_ACCENT    // Primary highlight
CLR_HIGHLIGHT // Selection border
CLR_TEXT      // Primary text
CLR_SUBTEXT   // Secondary text
```

### Add a New Hardware Module

1. Create directory under `main/modules/foo/`
2. Add source files to `main/CMakeLists.txt` — both `SRCS` and `INCLUDE_DIRS`
3. Add `#include` in `main/main.c` and call init function in `app_main()`
4. If it uses a hardware peripheral, add to `REQUIRES` in `main/CMakeLists.txt`

---

## 5. Debugging

### Common Crash Signatures

| Crash | Likely Cause | Fix |
|-------|-------------|-----|
| `wifi:alloc eb fail` | WiFi out of DMA memory | Reduce LVGL buffer or increase PSRAM allocation |
| LVGL freeze after screen switch | LVGL 64KB heap exhausted | Missing `lv_obj_del(old_scr)` in screen create |
| Crash on 2nd app entry | Dangled static pointer | Reset all static widget ptrs to NULL on create |
| `Guru Meditation Error` | Stack overflow | Increase task stack size |
| `cJSON_Parse` returns NULL | No PSRAM for cJSON | cJSON allocators must force `MALLOC_CAP_SPIRAM` |
| LCD horizontal stripes | Single buffer / power | Enable double buffering, check VCOM/VRH in driver |

### Serial Log Tags

| Tag | What to look for |
|-----|-----------------|
| `MAIN` | Key handler dispatch, init sequence progress |
| `APP_MGR` | App lifecycle (launch/return) |
| `LAUNCHER` | Home screen updates |
| `WEATHER` | "API key not set" = needs config, or JSON parse errors |
| `NES` | Frame timing, ROM loading |
| `WIFI` | Connection state, provisioning status |
| `BLE_HID` | Pairing events, `AUTH OK` or `AUTH ERROR` |
| `LVGL` | Display buffer swaps, DMA errors |
| `BOX_AUDIO` | Mic diagnostics: non-zero range = OK, zero = no data |
| `RG_WRAP` | In-process retro-go adapter |

### Memory Diagnostics
```c
ESP_LOGI(TAG, "DMA free: %zu", heap_caps_get_free_size(MALLOC_CAP_DMA));
ESP_LOGI(TAG, "PSRAM free: %zu", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
heap_caps_print_heap_info(MALLOC_CAP_8BIT);
lv_mem_monitor_t mm; lv_mem_monitor(&mm);
// Check: mm.free_size > 0, mm.frag_pct < 50%
```

---

## 6. Kconfig / sdkconfig Guidance

WARN the user about these critical non-default settings:

| Config | Value | Why |
|--------|-------|-----|
| `CONFIG_FREERTOS_HZ` | 1000 | 1ms tick (not 10ms). Timing from other projects runs 10x faster |
| `CONFIG_LV_COLOR_16_SWAP` | y | RGB565 byte-swapped for ST7789 |
| `CONFIG_SPIRAM_USE_CAPS_ALLOC` | y | **Never change to MALLOC** — DMA components crash |
| `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` | n | NES emu starves idle task |
| `CONFIG_BT_BLUEDROID_ENABLED` | y | Must be Bluedroid (NOT NimBLE) for HID |

**Never delete `build/` without backing up `sdkconfig`** — `sdkconfig.defaults` alone doesn't restore LVGL fonts, partition, or PSRAM config.

---

## 7. Third-Party Dependencies

### Submodules (need `--recursive` clone):
- `components/lvgl` — https://github.com/lvgl/lvgl (v8.4)
- `components/esp-wifi-connect` — https://github.com/78/esp-wifi-connect

### Managed components (downloaded when `IDF_COMPONENT_MANAGER=1`):
- `espressif/zlib` — weather module decompression
- `espressif/esp_codec_dev` — audio codec abstraction (already vendored)

### Standalone sub-projects (not part of main build):
- `retro-go-firmware/` — Standalone OTA_0 firmware. Requires emulator cores from [ducalex/retro-go](https://github.com/ducalex/retro-go). Set `RETRO_GO_CORE_DIR` in its CMakeLists.txt.
- **XiaoZhi AI (ota_1)** — NOT included in this repo. Clone [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) separately:
  ```bash
  git clone https://github.com/78/xiaozhi-esp32.git
  cd xiaozhi-esp32
  # Create a board definition or use an existing one
  idf.py set-target esp32s3
  idf.py build
  python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
    write_flash 0x810000 build/xiaozhi.bin
  ```
  The ESP-Handheld menu's "小智" button reboots into ota_1 to launch it.

---

## 8. What NOT To Do

- **Don't remove dead code** (`modules/nes/`, `components/nes_core/`, `app/rom_browser.c`) — kept as reference
- **Don't enable `CONFIG_SPIRAM_USE_MALLOC=y`** — breaks DMA peripherals
- **Don't commit `managed_components/`** — auto-generated; commit `dependencies.lock` instead
- **Don't build under MSYS2/Mingw** — ESP-IDF refuses with MSYSTEM detection
- **Don't delete `sdkconfig` without backup** — hard to reconstruct
- **Don't add new features without verifying build** — no test suite, hardware-in-the-loop only
