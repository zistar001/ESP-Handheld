# Build Guide

## Prerequisites

- **ESP-IDF v5.5.4** with `esp32s3` target installed
- **Git** with Git LFS (for submodules)
- **Python 3.11+**
- **CMake** + **Ninja** (bundled with ESP-IDF)

## Installing ESP-IDF

### Windows

1. Download the ESP-IDF installer from [espressif.com](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html)
2. Choose **v5.5.4** and enable **ESP32S3** target
3. After installation, launch the **ESP-IDF PowerShell** shortcut

### Linux

```bash
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

### macOS

```bash
git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

## Building

### Quick Setup (Recommended)

```powershell
# Windows
.\scripts\setup.ps1
```

```bash
# Linux/macOS
. $IDF_PATH/export.sh
./scripts/setup.sh
```

### Manual Build

```bash
# 1. Clone with submodules
git clone --recursive https://github.com/YOUR_USERNAME/ESP-Handheld.git
cd ESP-Handheld
git submodule update --init --recursive

# 2. Set up environment (ESP-IDF PowerShell on Windows, or export.sh on Linux)
. $IDF_PATH/export.sh

# 3. Configure (optional — API keys, etc.)
idf.py menuconfig
# → "ESP-Handheld Configuration" → set Weather API Key

# 4. Build
export IDF_COMPONENT_MANAGER=0   # 0 for faster build; 1 if weather needs zlib
idf.py build
```

### Common Build Options

| Option | Description |
|--------|-------------|
| `IDF_COMPONENT_MANAGER=0` | **Default.** Offline build, no registry downloads. Weather (zlib) will fail. |
| `IDF_COMPONENT_MANAGER=1` | Enable component registry. Required for weather module (zlib). |

### Build Output

After a successful build, you'll find:
- `build/esp_handheld.bin` — Main application (flash to 0x10000)
- `build/bootloader/bootloader.bin` — Bootloader (flash to 0x0)
- `build/partition_table/partition-table.bin` — Partition table (flash to 0x8000)
- `build/ota_data_initial.bin` — OTA boot data (flash to 0xd000)

## Troubleshooting

### `cc1.exe: CreateProcess: No such file or directory`
Parallel build issue on Windows. Retry or limit concurrency:
```powershell
idf.py build -j2
```

### `component_manager: unknown command`
The component manager is not installed. Set `IDF_COMPONENT_MANAGER=0` or install it:
```powershell
pip install idf-component-manager
```

### Build fails on MSYS2/Mingw
ESP-IDF detects MSYS2 and refuses to run. Use PowerShell or cmd.exe instead. If running from bash, clear the MSYSTEM variable:
```powershell
$env:MSYSTEM=''
```

### `CONFIG_VFS_SUPPORT_IO` errors
ESP-IDF v5.5.4 has changed some VFS defaults. Make sure you're using the correct sdkconfig.defaults.
