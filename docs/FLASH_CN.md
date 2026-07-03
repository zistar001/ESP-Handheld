# 烧录指南

## 连接

通过 USB 连接你的 ESP32-S3 设备。固件使用以下参数：
- **波特率：** 921600（监控用 115200）
- **Flash 模式：** dio
- **Flash 大小：** 16MB
- **Flash 频率：** 80MHz

### 查找串口

| 系统 | 典型端口 |
|------|---------|
| Windows | `COM3`、`COM4` 等（查看设备管理器） |
| Linux | `/dev/ttyACM0`、`/dev/ttyUSB0` |
| macOS | `/dev/cu.usbmodem*`、`/dev/cu.wchusbserial*` |

## 首次全量烧录

```bash
python -m esptool --chip esp32s3 -p (端口) -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

> **重要：** `ota_data_initial.bin`（烧录到 0xd000）用于选择 factory 分区。
> 没有它，设备可能启动到错误的分区（例如 XiaoZhi AI）。

## 仅更新应用（快速烧录）

```bash
python -m esptool --chip esp32s3 -p (端口) -b 921600 \
  write_flash 0x10000 build/esp_handheld.bin
```

## 烧录 Retro-Go 游戏系统（ota_0）

```bash
# 先编译
cd retro-go-firmware
powershell -ExecutionPolicy Bypass -File build_rg.ps1

# 烧录
python -m esptool --chip esp32s3 -p (端口) -b 921600 \
  write_flash 0x410000 build/retro-go-firmware.bin
```

## 烧录 XiaoZhi AI（ota_1）

```bash
# 编译 XiaoZhi
cd xiaozhi-zistar-main
idf.py build

# 烧录应用到 ota_1
python -m esptool --chip esp32s3 -p (端口) -b 921600 \
  write_flash 0x810000 build/xiaozhi.bin
```

## SPIFFS 资源分区

```bash
python $IDF_PATH/components/spiffs/spiffsgen.py 0x3F0000 assets/ build/assets.bin
python -m esptool --chip esp32s3 -p (端口) -b 921600 \
  write_flash 0xC10000 build/assets.bin
```

## 串口监控

```bash
idf.py -p (端口) monitor
# 按 Ctrl+] 退出
```

## 系统切换

设备可运行三个独立的固件：

| 菜单选项 | 启动分区 | 系统 |
|---------|---------|------|
| "游戏" | ota_0 (0x410000) | Retro-Go 游戏系统 |
| "小智" | ota_1 (0x810000) | XiaoZhi AI |
| START+B（从 Retro-Go/XiaoZhi） | factory (0x10000) | ESP-Handheld |
