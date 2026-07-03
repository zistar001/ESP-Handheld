# Hardware Adaptation Guide

## Reference PCB (Rev 2)

This firmware was developed for a custom ESP32-S3 handheld PCB (Rev 2).
Below are the complete pin mappings.

### Pin Map

| Function | Interface | Pin | Notes |
|----------|-----------|-----|-------|
| **LCD BL** | — | GPIO43 | PWM backlight |
| **LCD DC** | SPI2_HOST | GPIO44 | Data/Command |
| **LCD CS** | SPI2_HOST | GPIO42 | Chip select |
| **LCD SCK** | SPI2_HOST | GPIO21 | SPI clock |
| **LCD MOSI** | SPI2_HOST | GPIO14 | SPI data |
| **LCD RST** | — | GPIO13 | Reset |
| **SD CS** | SPI3_HOST | GPIO38 | Chip select |
| **SD CLK** | SPI3_HOST | GPIO40 | SPI clock |
| **SD MOSI** | SPI3_HOST | GPIO39 | SPI data |
| **SD MISO** | SPI3_HOST | GPIO41 | SPI data |
| **MIC BCLK** | I2S_NUM_0 | GPIO11 | MSM261 bit clock |
| **MIC WS** | I2S_NUM_0 | GPIO12 | MSM261 word select |
| **MIC DATA** | I2S_NUM_0 | GPIO10 | MSM261 data |
| **AMP BCLK** | I2S_NUM_1 | GPIO46 | MAX98357 bit clock |
| **AMP LRC** | I2S_NUM_1 | GPIO9 | MAX98357 left/right clock |
| **AMP DIN** | I2S_NUM_1 | GPIO3 | MAX98357 data |
| **I2C SDA** | I2C | GPIO1 | IMU + AHT20 |
| **I2C SCL** | I2C | GPIO2 | IMU + AHT20 |
| **KEY UP** | — | GPIO16 | Pull-up, active low |
| **KEY DOWN** | — | GPIO15 | Pull-up, active low |
| **KEY LEFT** | — | GPIO17 | Pull-up, active low |
| **KEY RIGHT** | — | GPIO7 | Pull-up, active low |
| **KEY A** | — | GPIO4 | Pull-up, active low |
| **KEY B** | — | GPIO5 | Pull-up, active low |
| **KEY START** | — | GPIO6 | Pull-up, active low |
| **BOOT** | — | GPIO0 | Boot mode select |
| **LED** | — | GPIO18 | Indicator LED |

### I2C Address Map

| Device | Address | Notes |
|--------|---------|-------|
| AHT20 | 0x38 | Temp/humidity sensor |
| LSM6DS3TR-C | 0x6A | IMU (SA0=VDD) |

### Adapting to Your PCB

1. **Edit `main/bsp/bsp_board.h`** — Change the `BSP_*` GPIO definitions to match your pins
2. **If your display is different**, also update:
   - `main/bsp/st7789_driver.c` — Display initialization sequence
   - `main/CMakeLists.txt` — If using a different display driver
3. **If your audio setup is different:**
   - `main/modules/audio/box_audio_codec.c` — I2S configuration
   - If using an external codec (ES8311/etc.), restore or adapt the old codec drivers
4. **Key mapping:**
   - The 7 physical keys use a simple GPIO matrix. Update pin definitions in `bsp_board.h`
   - Key remapping is available at runtime via Settings → Key Mapping

### Display Notes

- ST7789 has 240×320 memory, glass shows 240×280 (rows 20-299)
- RGB565 colors are byte-swapped (ST7789 requires MSB-first)
- LVGL config at `components/lv_conf.h`
- Display driver at `main/bsp/st7789_driver.c`

### Audio Notes

- Two independent I2S controllers: I2S_NUM_0 (mic RX), I2S_NUM_1 (amp TX)
- No external audio codec chip on Rev 2
- Mic gain: software multiplier (0-37dB → 1.0x-70x)
- MAX98357 needs ~12ms stabilization after BCLK starts
