# XiaoZhi AI — ESP-Handheld Board Definition

The XiaoZhi firmware with the custom board definition is included in the ESP-Handheld repo at `xiaozhi-zistar-main/`. You don't need to clone it separately.

## How to Build

```bash
cd xiaozhi-zistar-main
idf.py set-target esp32s3          # first time only
idf.py build
# Flash to ota_1:
python -m esptool --chip esp32s3 -p (PORT) -b 921600 \
  write_flash 0x810000 build/xiaozhi.bin
```

## Hardware Configuration

| Item | Detail |
|------|--------|
| **SoC** | ESP32-S3 |
| **Display** | ST7789, 240×280, SPI2_HOST |
| **Speaker** | MAX98357 (I2S_NUM_1 TX) |
| **Microphone** | MSM261 (I2S_NUM_0 RX) |
| **Buttons** | A=PTT (GPIO4), UP=Vol+ (GPIO16), DOWN=Vol- (GPIO15) |
| **START+B combo** | Returns to ESP-Handheld (GPIO6+GPIO5, 150ms) |
| **LED** | GPIO18 |

## Files

| File | Purpose |
|------|---------|
| `config.h` | Pin definitions, display config, audio mode |
| `zistar_boards.cc` | Board class: init sequence, display/audio/button setup, combo key detection |
| `zistar_audio_codec.h` | Audio codec header (I2S simplex: separate controllers for mic + speaker) |
| `zistar_audio_codec.cc` | Audio codec implementation (16kHz, gain mapping, volume) |

## Key Notes

- Uses **NoAudioCodecSimplex**: MIC on I2S_NUM_0, SPK on I2S_NUM_1 (independent I2S controllers)
- Mic gain: software multiplier (37dB ≈ 70x), matches ESP-Handheld's box_audio_codec
- No battery ADC — always reports 100%
- The "zistar" name is derived from the GitHub username of the original developer
