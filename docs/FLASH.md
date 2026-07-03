# Flash Guide

## Connection

Connect your ESP32-S3 board via USB. The firmware uses:
- **Baud rate:** 921600 (115200 for monitor)
- **Flash mode:** dio
- **Flash size:** 16MB
- **Flash freq:** 80MHz

### Finding Your Port

| OS | Typical Port |
|----|------------|
| Windows | `COM3`, `COM4`, etc. (check Device Manager) |
| Linux | `/dev/ttyACM0`, `/dev/ttyUSB0` |
| macOS | `/dev/cu.usbmodem*`, `/dev/cu.wchusbserial*` |

## First-Time Full Flash

```bash
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

> **Important:** The `ota_data_initial.bin` at 0xd000 selects the factory partition.
> Without it, the device may boot into the wrong partition (e.g., XiaoZhi AI on ota_1).

## App-Only Flash (for quick updates)

```bash
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x10000 build/esp_handheld.bin
```

## Flashing Retro-Go (ota_0)

```bash
# Build first
cd retro-go-firmware
powershell -ExecutionPolicy Bypass -File build_rg.ps1

# Flash
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x410000 build/retro-go-firmware.bin
```

## Flashing XiaoZhi AI (ota_1)

```bash
# Build XiaoZhi
cd xiaozhi-zistar-main
idf.py build

# Flash app to ota_1
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x810000 build/xiaozhi.bin
```

## SPIFFS Assets

```bash
python $IDF_PATH/components/spiffs/spiffsgen.py 0x3F0000 assets/ build/assets.bin
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0xC10000 build/assets.bin
```

## Monitor

```bash
idf.py -p (PORT) monitor
# Press Ctrl+] to exit
```

## Switching Between Systems

The device can run three independent firmware images:

| Menu Option | Boot Partition | System |
|------------|---------------|--------|
| "游戏" | ota_0 (0x410000) | Retro-Go game system |
| "小智" | ota_1 (0x810000) | XiaoZhi AI |
| START+B (from Retro-Go/XiaoZhi) | factory (0x10000) | ESP-Handheld |
