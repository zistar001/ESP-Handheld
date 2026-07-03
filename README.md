# ESP-Handheld

[![Build](https://github.com/YOUR_USERNAME/ESP-Handheld/actions/workflows/build.yml/badge.svg)](https://github.com/YOUR_USERNAME/ESP-Handheld/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

An open-source firmware for **ESP32-S3 handheld gaming & utility devices**. Features a retro game launcher with NES emulation, LVGL-based UI, weather dashboard, BLE HID keyboard/mouse, WiFi audio streaming, fortune telling (I Ching), and a dual-firmware system with XiaoZhi AI.

## Features

- 🎮 **Retro Gaming** — In-process NES emulator (nofrendo) + standalone Retro-Go on ota_0 (NES, GB/GBC, SMS, PCE, SNES)
- 🖥️ **LVGL UI** — 240×280 ST7789 display with status bar, home screen, app grid, and settings
- 🌤️ **Weather Dashboard** — HeFeng API (7-day forecast, real-time conditions)
- ⌨️ **BLE HID** — Keyboard & mouse over Bluetooth (Bluedroid)
- 🎙️ **WiFi Audio** — Push-to-talk streaming to PC (UDP PCM)
- 🎵 **Spectrum Analyzer** — FFT visualizer with adjustable parameters
- 🎲 **Fortune Telling** — I Ching hexagrams + Xiao Liu Ren divination
- 🎨 **Theme System** — 6 built-in color themes, mix-and-match 5 color slots
- 🔄 **Dual Firmware** — Partition-switch to XiaoZhi AI or Retro-Go
- 💤 **Power Management** — Light Sleep (5-10mA standby)
- 🎚️ **Audio** — MAX98357 digital amp + MSM261 digital mic (I2S)

## Hardware Requirements

| Component | Specification |
|-----------|--------------|
| SoC | ESP32-S3 (any variant with 8MB PSRAM) |
| Flash | 16MB |
| PSRAM | 8MB Octal |
| Display | ST7789 240×280 SPI LCD |
| Audio Amp | MAX98357 (I2S) |
| Microphone | MSM261 (I2S digital mic) |
| IMU | LSM6DS3TR-C (optional) |
| Temp/Humidity | AHT20 (optional) |
| SD Card | SPI mode (optional) |
| Buttons | 7 physical keys (UP/DOWN/LEFT/RIGHT/A/B/START) |

> **See [docs/HARDWARE.md](docs/HARDWARE.md) for pin mappings and PCB adaptation guide.**

## Quick Start

### Prerequisites

1. Install **ESP-IDF v5.5.4** for target `esp32s3`:
   - [Windows Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html)
   - [Linux/macOS Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/linux-macos-setup.html)
2. Install **Git**

### Windows

```powershell
# From ESP-IDF PowerShell environment:
git clone --recursive https://github.com/YOUR_USERNAME/ESP-Handheld.git
cd ESP-Handheld
.\scripts\setup.ps1
```

To configure your Weather API key:
```powershell
.\scripts\setup.ps1 -Menuconfig
# Navigate to: ESP-Handheld Configuration → Weather API Key
```

### Linux / macOS

```bash
git clone --recursive https://github.com/YOUR_USERNAME/ESP-Handheld.git
cd ESP-Handheld
source $IDF_PATH/export.sh
./scripts/setup.sh
```

### Flashing

```bash
# Full flash (first time):
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin

# App-only (subsequent):
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x10000 build/esp_handheld.bin
```

> Replace `(PORT)` with your serial port: `COM3` on Windows, `/dev/ttyACM0` on Linux.

## Services You Need to Register

| Service | Required? | Purpose | Registration |
|---------|-----------|---------|-------------|
| [HeFeng Weather](https://devapi.qweather.com) | Optional | Weather display (disabled if not configured) | Free API key |

**Configure via:** `idf.py menuconfig` → "ESP-Handheld Configuration" → "Weather API Key"

> See [docs/API_KEYS.md](docs/API_KEYS.md) for details.

## Partition Layout

| Partition | Offset | Size | Content |
|-----------|--------|------|---------|
| factory | 0x10000 | 4MB | ESP-Handheld (this firmware) |
| ota_0 | 0x410000 | 4MB | Retro-Go (standalone game system) |
| ota_1 | 0x810000 | 4MB | XiaoZhi AI firmware |
| assets | 0xC10000 | ~4MB | SPIFFS resources |

## Project Structure

```
main/
  main.c              — Entry point, init sequence, key handler
  bsp/                — Board support (st7789, sd_card, key_driver)
  app/                — App framework (app_manager, launcher, menu)
  modules/            — Feature modules (audio, imu, weather, ble, etc.)
  ui/                 — LVGL screens, components, display driver
components/
  lvgl/               — LVGL v8.4 (submodule)
  retro-core/         — NES emulator core (nofrendo)
  retro-go/           — Retro-Go runtime (display/input/audio drivers)
  esp-wifi-connect/   — WiFi provisioning (submodule)
```

## Documentation

- [Build Guide](docs/BUILD.md) — Detailed build instructions for all platforms
- [Flash Guide](docs/FLASH.md) — All flashing methods
- [Hardware Adaptation](docs/HARDWARE.md) — Pin mappings and PCB guide
- [API Keys](docs/API_KEYS.md) — Service registration guide

## License

This project is licensed under **GNU General Public License v2.0** — see [LICENSE](LICENSE).

**Why GPL v2?** The NES emulator core (nofrendo) is compiled and linked into the main firmware binary, and nofrendo is licensed under GPL v2. This requires the combined work to be distributed under GPL v2.

Includes components from:
- [nofrendo](https://github.com/ducalex/retro-go/tree/master/retro-core/components/nofrendo) — **GPL v2** (linked in main firmware)
- [Retro-Go](https://github.com/ducalex/retro-go) — GPL v2 (standalone ota_0 firmware, separate binary)
- [LVGL](https://github.com/lvgl/lvgl) — MIT
- [esp-wifi-connect](https://github.com/78/esp-wifi-connect) — No explicit license (submodule reference)
- [XiaoZhi AI](https://github.com/78/xiaozhi-esp32) — MIT (standalone ota_1 firmware, separate binary)
