# 编译指南

## 前置条件

- **ESP-IDF v5.5.4**（已安装 `esp32s3` 目标）
- **Git**（用于克隆和子模块）
- **Python 3.11+**
- **CMake** + **Ninja**（ESP-IDF 自带）

## 安装 ESP-IDF

### Windows

1. 从 [espressif.com](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/windows-setup.html) 下载 ESP-IDF 安装程序
2. 选择 **v5.5.4** 版本，勾选 **ESP32S3** 目标
3. 安装完成后，从开始菜单启动 **ESP-IDF PowerShell**

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

## 编译

### 快速开始（推荐）

```powershell
# Windows
.\scripts\setup.ps1
```

```bash
# Linux/macOS
. $IDF_PATH/export.sh
./scripts/setup.sh
```

### 手动编译

```bash
# 1. 克隆仓库（包含子模块）
git clone --recursive https://github.com/zistar001/ESP-Handheld.git
cd ESP-Handheld
git submodule update --init --recursive

# 2. 设置 ESP-IDF 环境（Windows 用 ESP-IDF PowerShell，Linux 用 export.sh）
. $IDF_PATH/export.sh

# 3. 配置（可选 — 设置 API Key 等）
idf.py menuconfig
# → "ESP-Handheld Configuration" → 设置 Weather API Key

# 4. 编译
export IDF_COMPONENT_MANAGER=0   # 0 = 离线编译（默认）；1 = 天气模块需要 zlib
idf.py build
```

### 编译选项

| 选项 | 说明 |
|--------|------|
| `IDF_COMPONENT_MANAGER=0` | **默认。** 离线编译，不从注册表下载组件。天气模块（zlib）会编译失败。 |
| `IDF_COMPONENT_MANAGER=1` | 启用组件注册表。天气模块需要此选项来下载 zlib。 |

### 编译产物

编译成功后：
- `build/esp_handheld.bin` — 主应用程序（烧录到 0x10000）
- `build/bootloader/bootloader.bin` — 引导程序（烧录到 0x0）
- `build/partition_table/partition-table.bin` — 分区表（烧录到 0x8000）
- `build/ota_data_initial.bin` — OTA 启动数据（烧录到 0xd000）

## 常见问题

### `cc1.exe: CreateProcess: No such file or directory`
Windows 并发编译问题。重试或限制并发数：
```powershell
idf.py build -j2
```

### `component_manager: unknown command`
组件管理器未安装。设 `IDF_COMPONENT_MANAGER=0` 或安装：
```powershell
pip install idf-component-manager
```

### MSYS2/Mingw 下编译失败
ESP-IDF 检测到 MSYS2 环境会拒绝运行。请使用 PowerShell 或 cmd.exe。如果在 bash 中运行，清除 MSYSTEM 变量：
```powershell
$env:MSYSTEM=''
```
