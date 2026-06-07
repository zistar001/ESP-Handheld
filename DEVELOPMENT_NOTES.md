# ESP_BSP — Development Notes

**Last Updated:** 2026-06-08
**Current Branch:** codex/nofrendo-migration
**Current Commit:** 723029e (Optimize: disable DFS, double-buffer LVGL, auto-connect WiFi, remove legacy BLE)
**Build Status:** ✅ Compiles and flashes
**Game Status:** ✅ NES playable, SD card working, exit/re-entry stable

---

## Project Overview

ESP32-S3 handheld device (ST7789 240x280 LCD + ES8311 audio + 7-key GPIO).
ESP-IDF v5.5.4 project, entry point `main/main.c`.
Platform: 16MB flash, 8MB PSRAM (Octal).

### Dual-System Partitions (custom partitions.csv)

| Partition | Offset   | Size   | Purpose          |
|-----------|----------|--------|------------------|
| nvs       | 0x9000   | 16K    | NVS              |
| otadata   | 0xD000   | 8K     | OTA data         |
| phy_init  | 0xF000   | 4K     | PHY init         |
| factory   | 0x10000  | 4MB    | ESP_BSP firmware |
| ota_0     | 0x410000 | 4MB    | OTA slot 0       |
| ota_1     | 0x810000 | 4MB    | OTA slot 1       |
| assets    | 0xC10000 | ~4MB   | SPIFFS assets    |

**Important:** Flash with `ota_data_initial.bin` at 0xd000 to ensure boot from factory.

---

## NES Game Module — Current Working Implementation

### Architecture

```
Main (Core 0)                     NES (Core 1)
───────────────                    ──────────────
app_manager                       game_task
  └─ rom_browser                    ├─ load_rom_file (SPI SD → PSRAM)
       └─ nes_start()               ├─ rom_getheader / rom_loadrom
            └─ nes_game_start()     ├─ nes_create / build_address_handlers
                 └─ xTaskCreate ──→ ├─ nes_renderframe(true) — 60fps loop
                                    └─ xTaskNotify(s_vid_task)

video_task (Core 0)               audio_timer (Core 0, FreeRTOS timer)
  ├─ xTaskNotifyWait               └─ apu_process → I2S write
  ├─ Line-by-line SPI transfer
  └─ on_game_exit → app_manager_return
```

### Key Design Decisions

1. **SD card first, LCD second**: SD initializes SPI bus at 400kHz (required for init), LCD joins later at 60MHz.
   - `sd_card_init()` creates SPI2_HOST bus
   - `st7789_init()` detects bus already exists, skips to panel IO

2. **Line-by-line video rendering**: Internal SRAM `line_buf[240]` avoids PSRAM DMA cache coherency issues.
   ```c
   uint16_t line_buf[LINE_PIXELS];  // stack, internal SRAM — DMA-safe
   st7789_spi_lock();
   for (int y = 0; y < 240; y++) {
       // convert palette → line_buf
       esp_lcd_panel_draw_bitmap(panel, 0, voff + y, 240, voff + y + 1, line_buf);
   }
   st7789_spi_unlock();
   ```

3. **Frame pacing**: vTaskDelay for coarse sleep (10ms granularity), spin-wait for µs precision:
   ```c
   const int32_t frame_us = 16667;  // 1000000/60
   if (remain > 11000) vTaskDelay(pdMS_TO_TICKS((remain - 10000) / 1000));
   while ((int32_t)(esp_timer_get_time() - t0) < frame_us) { asm volatile("nop"); }
   ```

4. **Task Watchdog**: `esp_task_wdt_reset()` called at each frame iteration — the spin-wait prevents idle task from running on Core 1.

### Exit Flow (Correct Pattern)

B+START held 3 frames → game_task sets `s_running=false` → cleanup → signals `s_game_done_sem` → notifies `s_vid_task` → deletes itself.

video_task wakes → calls `on_game_exit()`:
```c
static void on_game_exit(void) {
    ui_display_set_nes_active(false);   // restore LVGL rendering
    if (app_manager_get_state() == APP_STATE_RUNNING) {
        app_manager_return();            // MENU transition
    }
}
```

**Critical:** `nes_game_set_exit_callback(on_game_exit)` must be called in `nes_start()` (not just `nes_wrapper_init()`) because `s_exit_callback` is set to NULL after each use in `video_task`.

### Race Condition Prevention

| Flag | Purpose |
|------|---------|
| `s_running` | Game loop active? Set by game_task, cleared by stop |
| `s_abort` | Stop requested during ROM loading? Set by `nes_game_stop()`, checked by game_task before entering loop |
| `s_game_done_sem` | Binary semaphore: game_task gives on cleanup, `nes_game_stop()` takes to synchronize |

Stop during ROM loading flow:
1. `nes_game_stop()` → sets `s_abort=true`, `s_running=false` → waits on `s_game_done_sem`
2. game_task finishes loading → checks `s_abort` → `goto done` → gives semaphore → deletes
3. `nes_game_stop()` gets semaphore → clears `s_abort` → returns clean

### Hardware Pin Configuration

```
LCD:    BL=IO9  DC=IO10 CS=IO11 SCK=IO12 MOSI=IO13 RST=IO14
SD:     CS=IO18 (飞线) MISO=IO8   (shared SCK/MOSI with LCD)
Audio:  MCLK=IO40 BCLK=IO39 WS=IO45 DOUT=IO48 DIN=IO21
        Codec I2C: SDA=IO38 SCL=IO47
Sensor: I2C SDA=IO2 SCL=IO1
Keys:   UP=IO5 DOWN=IO6 LEFT=IO4 RIGHT=IO7 A=IO17 B=IO16 START=IO15
```

---

## Build & Flash

### Prerequisites
- ESP-IDF v5.5.4, target esp32s3
- Python: `C:\Users\Administrator\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe`
- Serial: COM7 (115200 monitor / 921600 flash)
- **IMPORTANT:** Build must run from cmd.exe or PowerShell, NOT MSYS2/Mingw bash
  (ESP-IDF detects MSYSTEM env var and refuses to run)

### PowerShell Build Script Pattern
```powershell
$env:MSYSTEM=''  # Clear MSYS2 detection
$env:IDF_PATH='D:\Espressif\frameworks\esp-idf-v5.5.4'
$env:IDF_TOOLS_PATH='D:\Espressif\tools'
$env:IDF_COMPONENT_MANAGER='0'
$env:PATH='...\cmake\bin;...\ninja;...\xtensa-esp-elf\bin;...\python\Scripts;' + $env:PATH
python D:\Espressif\frameworks\esp-idf-v5.5.4\tools\idf.py build
```

### Flash Command (full, including otadata)
```bash
python -m esptool --chip esp32s3 -p COM7 -b 921600 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x10000 build/esp_handheld.bin
```

### sdkconfig
- DO NOT delete `build/` directory without backing up `sdkconfig`
- The `sdkconfig` at project root contains critical settings (LVGL fonts, partition scheme, PSRAM mode)
- `idf.py set-target esp32s3` regenerates from `sdkconfig.defaults` which is incomplete

---

## Known Issues

1. **Color may be incorrect**: `esp_lcd_panel_invert_color(panel, true)` in `st7789_init()`. If colors look inverted, try `false`.
2. **Unsupported NES mappers**: Only mappers 0,1,2,3,4,7,11,15, etc. are supported. ROMs with mapper 83 (Fatal Fury 2) etc. will fail with "Unsupported mapper".
3. **Task WDT on CPU 0**: Occasional when LVGL operations happen in key callback. Mitigated by `esp_task_wdt_reset()` on Core 1 and priority adjustments. Non-critical.

---

## Git Branches

| Branch | HEAD | Description |
|--------|------|-------------|
| master | e9ed773 | Original NES module (incomplete CMakeLists.txt) |
| codex/nofrendo-migration | 723029e | Current: full UI + working game module |

### Commit History
```
723029e — Optimize: DFS disable, LVGL double-buffer, WiFi, remove BLE
e9ed773 — NES game module (gamer project emucore)
6e04d4a — sensor drivers + SD card SPI
a931d41 — GPIO key driver
0f95b8c — BSP + ST7789 driver
cb98dc7 — initial scaffold
```
