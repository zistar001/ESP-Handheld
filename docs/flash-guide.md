# ESP32-S3 智能小手柄 — 刷机指南

**最后更新:** 2026-06-08

## 硬件要求

- ESP32-S3 开发板/模组 (16MB Flash, 8MB Octal PSRAM)
- USB 数据线 (数据+电源)
- 电脑安装好 ESP-IDF 环境 (v5.5.4)
- COM7 串口（根据实际情况调整）

---

## 一、分区布局

| 分区 | 偏移 | 大小 | 用途 |
|------|------|------|------|
| bootloader | 0x0 | 32KB | 二级引导 |
| nvs | 0x9000 | 16KB | WiFi/RTC 配置 |
| otadata | 0xD000 | 8KB | OTA 启动选择 |
| phy_init | 0xF000 | 4KB | RF 校准 |
| **factory** | **0x10000** | **4MB** | **Game 固件 (ESP_BSP)** |
| ota_0 | 0x410000 | 4MB | Game OTA 升级备用 |
| **ota_1** | **0x810000** | **4MB** | **XiaoZhi AI 固件** |
| assets | 0xC10000 | ~4MB | SPIFFS 资源 |

**双系统逻辑:**
- **系统 A (Game):** factory 分区 — 手柄主界面、NES、BLE 远控、天气
- **系统 B (XiaoZhi AI):** ota_1 分区 — AI 语音交互
- 在菜单中切换会自动重启到另一个系统

---

## 二、构建 Game 固件

```bash
cd D:\code\claudecode\ESP_BSP

# 配置
idf.py set-target esp32s3

# 构建
idf.py build
```

构建产物:
- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`
- `build/ota_data_initial.bin`
- `build/esp_handheld.bin` (主固件, 偏移 0x10000)
- `build/assets.bin` (SPIFFS, 偏移 0xC10000)

**注意:** MSYS2/Mingw bash不支持ESP-IDF，必须用cmd.exe或PowerShell构建。

---

## 三、刷写 Game 固件 (完整烧录)

### 方法1: 使用构建脚本（推荐）

```bash
build.bat      # 编译+烧录
```

### 方法2: 手动完整烧录（含otadata）

```bash
# 擦除otadata分区（重置启动分区指向factory）
python -m esptool --chip esp32s3 -p COM7 -b 921600 erase_region 0xd000 0x2000

# 完整烧录
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

**重要:** 必须烧录`ota_data_initial.bin`到0xd000，否则可能启动到ota_1（小智AI）而非factory（Game）。

### 方法3: 首次烧录 (全片擦除)

```bash
idf.py -p COM7 erase-flash
idf.py -p COM7 flash
```

---

## 四、刷写 XiaoZhi AI 固件

**关键说明：** 小智固件是位置相关的（position-dependent），编译时链接地址由分区表决定。必须用**与 Game 固件一致的分区表**来编译小智，使其 app 分区定位在 0x810000。

### 步骤

**1. 给小智项目配置分区表**

将本项目的 `partitions.csv` 复制到小智 AI 项目目录：

```bash
cp partitions.csv /path/to/xiaozhi_project/
```

在小智项目目录确保 sdkconfig 使用自定义分区表：
```bash
idf.py menuconfig
# → Partition Table → Custom partition table CSV → partitions.csv
```

**2. 构建并烧录**

```bash
cd /path/to/xiaozhi_project
idf.py build
esptool.py -p COM7 -b 921600 write_flash 0x810000 build/xiaozhi.bin
```

> 注意：只烧录应用二进制，**不要**烧小智项目的 bootloader 或分区表。

---

## 五、双系统刷写步骤 (完整流程)

```bash
# 1. 全片擦除
idf.py -p COM7 erase-flash

# 2. 烧录 Game 固件 (含 bootloader + 分区表 + otadata + factory)
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin

# 3. 烧录小智 AI 到 ota_1 (0x810000)
esptool.py -p COM7 -b 921600 write_flash 0x810000 xiaozhi.bin
```

---

## 六、系统切换

| 操作路径 | 结果 |
|---------|------|
| 菜单 → **AI** | 重启进入 XiaoZhi AI (ota_1) |
| 小智 AI 内部 → **切换到手柄** | 重启进入 Game (factory) |
| 开机前按住 BOOT 键 | 进入下载模式 (刷机) |

**切换原理:** 调用 `esp_ota_set_boot_partition()` + `esp_restart()`，写入 otadata 分区标记下次启动的分区。

---

## 七、常见问题

### Q: 烧录后启动到小智AI而不是Game?

otadata指向了ota_1分区。重新烧录otadata：
```bash
esptool.py -p COM7 write_flash 0xd000 build/ota_data_initial.bin
```

### Q: 小智 AI 分区为空会怎样?

菜单选择 AI 时，若 ota_1 为空，会在日志输出 `Target partition not found`，不会重启。

### Q: 如何判断当前运行在哪个系统?

进入 **关于** 页面可查看当前系统名称和分区名。或者在串口日志中搜索 `Running from:`。

### Q: 刷错分区导致无法启动?

```bash
# 重新全片擦除后从头烧录
idf.py -p COM7 erase-flash
idf.py -p COM7 flash
```

---

## 八、串口监视

```bash
idf.py -p COM7 monitor
# 或直接使用Python
python -c "import serial; s=serial.Serial('COM7',115200); exec('while 1: print(s.readline().decode().strip())')"
```

退出: `Ctrl+]` (idf.py) / `Ctrl+C` (Python)

从监视器中重启: `Ctrl+T` → `Ctrl+R`
