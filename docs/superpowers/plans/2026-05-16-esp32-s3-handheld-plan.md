# ESP32-S3 智能小手柄 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建基于 ESP32-S3 的智能小手柄，包含 NES 模拟器、小智 AI、PC 远程控制、传感器等功能

**Architecture:** ESP-IDF + ESP-BSP 框架，LVGL 图形界面，FreeRTOS 双核调度，应用管理器状态机驱动模块切换

**Tech Stack:** ESP-IDF v5.x, ESP-BSP, LVGL 8.x, FreeRTOS, C/C++, CMake

---

## Project File Structure

```
D:\code\claudecode\ESP_BSP\
├── CMakeLists.txt                    # 顶层 CMake
├── sdkconfig.defaults                # 默认配置
├── main/
│   ├── CMakeLists.txt                # main 组件 CMake
│   ├── main.c                        # 程序入口
│   ├── app/
│   │   ├── app_manager.c             # 应用状态机
│   │   ├── app_manager.h
│   │   ├── launcher.c                # 主界面逻辑
│   │   ├── launcher.h
│   │   ├── menu.c                    # 菜单逻辑
│   │   └── menu.h
│   ├── bsp/
│   │   ├── bsp_board.c               # 自定义 BSP
│   │   ├── bsp_board.h
│   │   ├── st7789_driver.c           # ST7789 屏幕驱动
│   │   ├── st7789_driver.h
│   │   ├── key_driver.h              # 按键驱动
│   │   ├── sd_card.c                 # SD 卡驱动
│   │   └── sd_card.h
│   ├── modules/
│   │   ├── imu/
│   │   │   ├── imu_driver.c          # LSM6DS3TR-C 驱动
│   │   │   └── imu_driver.h
│   │   ├── sensor/
│   │   │   ├── aht20_driver.c        # AHT20 驱动
│   │   │   └── aht20_driver.h
│   │   ├── nes/
│   │   │   ├── nes_wrapper.c         # NES 模拟器适配
│   │   │   └── nes_wrapper.h
│   │   ├── xiaozhi/                  # 小智 AI (后续移植)
│   │   └── pc_remote/
│   │       ├── ble_hid.c             # BLE HID 键鼠
│   │       ├── ble_hid.h
│   │       ├── air_mouse.c           # 空中飞鼠
│   │       ├── air_mouse.h
│   │       ├── wifi_audio.c          # WiFi 语音传输
│   │       └── wifi_audio.h
│   ├── ui/
│   │   ├── screens/
│   │   │   ├── main_screen.c         # 主界面 LVGL
│   │   │   ├── main_screen.h
│   │   │   ├── menu_screen.c         # 菜单 LVGL
│   │   │   ├── menu_screen.h
│   │   │   ├── settings_screen.c     # 设置 LVGL
│   │   │   └── settings_screen.h
│   │   └── components/
│   │       ├── status_bar.c          # 状态栏组件
│   │       └── status_bar.h
│   └── idf_component.yml
└── components/
    └── nes_emulator/                 # NES 模拟器 (git submodule)
```

---

## Phase 1: 工程框架 + 点亮屏幕

### Task 1.1: 初始化 ESP-IDF 项目

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\CMakeLists.txt`
- Create: `D:\code\claudecode\ESP_BSP\main\CMakeLists.txt`
- Create: `D:\code\claudecode\ESP_BSP\main\idf_component.yml`
- Create: `D:\code\claudecode\ESP_BSP\sdkconfig.defaults`
- Create: `D:\code\claudecode\ESP_BSP\main\main.c`

- [ ] **Step 1: 检查 IDF 环境**

Run: `idf.py --version`
Expected: ESP-IDF v5.x

- [ ] **Step 2: 创建顶层 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_handheld)
```

- [ ] **Step 3: 创建 main/CMakeLists.txt**

```cmake
idf_component_register(
    SRCS "main.c"
          "bsp/bsp_board.c" "bsp/st7789_driver.c"
          "app/app_manager.c" "app/launcher.c" "app/menu.c"
          "ui/screens/main_screen.c" "ui/screens/menu_screen.c"
          "ui/screens/settings_screen.c" "ui/components/status_bar.c"
          "modules/imu/imu_driver.c" "modules/sensor/aht20_driver.c"
          "modules/pc_remote/ble_hid.c" "modules/pc_remote/air_mouse.c"
          "modules/pc_remote/wifi_audio.c" "modules/nes/nes_wrapper.c"
          "bsp/sd_card.c"
    INCLUDE_DIRS "."
                 "bsp" "app" "ui/screens" "ui/components"
                 "modules/imu" "modules/sensor" "modules/pc_remote"
                 "modules/nes"
    REQUIRES driver esp_timer lvgl fatfs spi_flash
)
```

- [ ] **Step 4: 创建 main/idf_component.yml**

```yaml
dependencies:
  espressif/esp-bsp: "^1.0.0"
  lvgl/lvgl: "^8.3.0"
  idf: ">=5.0"
```

- [ ] **Step 5: 创建 sdkconfig.defaults**

```
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240=y
CONFIG_SPIRAM_MODE_QUAD=y
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_USE_CAPS_ALLOC=y
CONFIG_LVGL_MEM_SIZE=16384
CONFIG_FATFS_LFN_STACK=y
```

- [ ] **Step 6: 创建 main.c (空壳)**

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_board.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Handheld starting...");
    bsp_board_init();
    ESP_LOGI(TAG, "System ready");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

- [ ] **Step 7: 编译**

Run: `idf.py set-target esp32s3 && idf.py build`
Expected: 编译成功

- [ ] **Step 8: 提交**

```bash
git init
git add -A
git commit -m "chore: initial ESP-IDF project scaffold"
```

---

### Task 1.2: 自定义 BSP — 引脚映射 + ST7789

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\bsp_board.h`
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\bsp_board.c`
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\st7789_driver.h`
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\st7789_driver.c`

- [ ] **Step 1: 创建 bsp_board.h**

```c
#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LCD */
#define BSP_LCD_BL      GPIO_NUM_9
#define BSP_LCD_DC      GPIO_NUM_10
#define BSP_LCD_CS      GPIO_NUM_11
#define BSP_LCD_CLK     GPIO_NUM_12
#define BSP_LCD_MOSI    GPIO_NUM_13
#define BSP_LCD_RST     GPIO_NUM_14
#define BSP_LCD_HOST    SPI2_HOST

/* SD */
#define BSP_SD_CS       GPIO_NUM_8

/* I2C */
#define BSP_I2C_SCL     GPIO_NUM_1
#define BSP_I2C_SDA     GPIO_NUM_2
#define BSP_I2C_CLK     100000

/* I2S 输出 (ES8311) */
#define BSP_I2S_BCK     GPIO_NUM_3
#define BSP_I2S_WS      GPIO_NUM_46

/* I2S 输入 (ES7210) */
#define BSP_MIC_BCK     GPIO_NUM_38
#define BSP_MIC_WS      GPIO_NUM_37
#define BSP_MIC_DATA0   GPIO_NUM_39
#define BSP_MIC_DATA1   GPIO_NUM_40

/* 按键 */
#define BSP_KEY_UP      GPIO_NUM_5
#define BSP_KEY_DOWN    GPIO_NUM_6
#define BSP_KEY_LEFT    GPIO_NUM_4
#define BSP_KEY_RIGHT   GPIO_NUM_7
#define BSP_KEY_A       GPIO_NUM_16
#define BSP_KEY_B       GPIO_NUM_15
#define BSP_KEY_START   GPIO_NUM_17

esp_err_t bsp_board_init(void);
esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_backlight_set(uint8_t brightness_percent);
esp_err_t bsp_i2c_init(void);

#ifdef __cplusplus
}
#endif
#endif
```

- [ ] **Step 2: 创建 bsp_board.c**

```c
#include "bsp_board.h"
#include "st7789_driver.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "BSP";
static spi_device_handle_t lcd_spi = NULL;
spi_device_handle_t bsp_get_lcd_spi(void) { return lcd_spi; }

esp_err_t bsp_lcd_backlight_set(uint8_t brightness_percent) {
    if (brightness_percent > 100) brightness_percent = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (brightness_percent * 8192) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    return ESP_OK;
}

esp_err_t bsp_lcd_init(void) {
    ESP_LOGI(TAG, "LCD init...");
    gpio_set_direction(BSP_LCD_BL, GPIO_MODE_OUTPUT);
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_ch = {
        .gpio_num = BSP_LCD_BL, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0,
        .duty = 0, .hpoint = 0
    };
    ledc_channel_config(&ledc_ch);
    bsp_lcd_backlight_set(50);

    gpio_set_direction(BSP_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BSP_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_LCD_MOSI, .miso_io_num = -1,
        .sclk_io_num = BSP_LCD_CLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 280 * 2 + 8
    };
    spi_bus_initialize(BSP_LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 60 * 1000 * 1000,
        .mode = 0, .spics_io_num = BSP_LCD_CS,
        .queue_size = 7, .flags = SPI_DEVICE_NO_DUMMY
    };
    spi_bus_add_device(BSP_LCD_HOST, &dev_cfg, &lcd_spi);

    st7789_init(lcd_spi);
    ESP_LOGI(TAG, "LCD init done");
    return ESP_OK;
}

esp_err_t bsp_i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_I2C_SDA, .scl_io_num = BSP_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BSP_I2C_CLK
    };
    return i2c_param_config(I2C_NUM_0, &conf) |
           i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t bsp_board_init(void) {
    ESP_LOGI(TAG, "BSP init");
    bsp_lcd_init();
    bsp_i2c_init();
    return ESP_OK;
}
```

- [ ] **Step 3: 创建 st7789_driver.h**

```c
#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include "esp_err.h"
#include "driver/spi_master.h"

#define ST7789_WIDTH   240
#define ST7789_HEIGHT  280

typedef enum {
    ST7789_DIR_0, ST7789_DIR_90, ST7789_DIR_180, ST7789_DIR_270
} st7789_dir_t;

esp_err_t st7789_init(spi_device_handle_t spi);
esp_err_t st7789_set_direction(st7789_dir_t dir);
esp_err_t st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
esp_err_t st7789_write_ram(uint8_t *data, size_t len);
esp_err_t st7789_sleep_in(void);
esp_err_t st7789_sleep_out(void);

#endif
```

- [ ] **Step 4: 创建 st7789_driver.c**

```c
#include "st7789_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "ST7789";
static spi_device_handle_t spi_handle = NULL;

static void write_cmd(uint8_t c) {
    gpio_set_level(BSP_LCD_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &c, .flags = SPI_DEVICE_HALFDUPLEX };
    spi_device_transmit(spi_handle, &t);
}
static void write_data(uint8_t d) {
    gpio_set_level(BSP_LCD_DC, 1);
    spi_transaction_t t = { .length = 8, .tx_buffer = &d, .flags = SPI_DEVICE_HALFDUPLEX };
    spi_device_transmit(spi_handle, &t);
}
static void write_buf(uint8_t *data, size_t len) {
    gpio_set_level(BSP_LCD_DC, 1);
    spi_transaction_t t = { .length = len * 8, .tx_buffer = data, .flags = SPI_DEVICE_HALFDUPLEX };
    spi_device_transmit(spi_handle, &t);
}

esp_err_t st7789_init(spi_device_handle_t spi) {
    spi_handle = spi;
    write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));
    write_cmd(0x3A); write_data(0x05);  // 16-bit color
    write_cmd(0x36); write_data(0x00);  // MADCTL
    write_cmd(0x29);  // display on
    ESP_LOGI(TAG, "ST7789 ready");
    return ESP_OK;
}

esp_err_t st7789_set_direction(st7789_dir_t dir) {
    uint8_t vals[] = {0x00, 0x60, 0xC0, 0xA0};
    write_cmd(0x36); write_data(vals[dir]);
    return ESP_OK;
}

esp_err_t st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    write_cmd(0x2A);
    write_data(x1 >> 8); write_data(x1); write_data(x2 >> 8); write_data(x2);
    write_cmd(0x2B);
    write_data(y1 >> 8); write_data(y1); write_data(y2 >> 8); write_data(y2);
    write_cmd(0x2C);
    return ESP_OK;
}

esp_err_t st7789_write_ram(uint8_t *data, size_t len) {
    write_buf(data, len);
    return ESP_OK;
}

esp_err_t st7789_sleep_in(void)  { write_cmd(0x10); return ESP_OK; }
esp_err_t st7789_sleep_out(void) { write_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120)); return ESP_OK; }
```

- [ ] **Step 5: 修改 main.c — 显示测试色条**

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "st7789_driver.h"

static const char *TAG = "MAIN";

static void show_test_pattern(void) {
    size_t buf_size = ST7789_WIDTH * ST7789_HEIGHT * 2;
    uint8_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf) { ESP_LOGE(TAG, "No PSRAM"); return; }
    for (int y = 0; y < ST7789_HEIGHT; y++)
        for (int x = 0; x < ST7789_WIDTH; x++) {
            int i = (y * ST7789_WIDTH + x) * 2;
            uint16_t c = (x < 80) ? 0xF800 : (x < 160) ? 0x07E0 : 0x001F;
            buf[i] = c >> 8; buf[i+1] = c & 0xFF;
        }
    st7789_set_window(0, 0, ST7789_WIDTH-1, ST7789_HEIGHT-1);
    st7789_write_ram(buf, buf_size);
    free(buf);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting...");
    bsp_board_init();
    show_test_pattern();
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
```

- [ ] **Step 6: 编译并烧录验证**

Run: `idf.py build && idf.py -p PORT flash monitor`
Expected: 屏幕红/绿/蓝三色条纹

- [ ] **Step 7: 提交**

```bash
git add main/bsp/ main/main.c
git commit -m "feat: add custom BSP and ST7789 driver with test pattern"
```

---

## Phase 2: 按键 + LVGL + 主界面/菜单

### Task 2.1: 按键驱动

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\key_driver.h`
- Modify: `D:\code\claudecode\ESP_BSP\main\bsp\bsp_board.c`

- [ ] **Step 1: 创建 key_driver.h**

```c
#ifndef KEY_DRIVER_H
#define KEY_DRIVER_H
#include "esp_err.h"

typedef enum {
    KEY_UP=0, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_START, KEY_MAX
} key_id_t;

typedef void (*key_cb_t)(key_id_t key, bool pressed);

esp_err_t key_driver_init(key_cb_t cb);
void key_driver_scan_task(void *arg);

#endif
```

- [ ] **Step 2: 在 bsp_board.c 末尾追加按键实现**

```c
// 添加到 bsp_board.c
#include "key_driver.h"

static key_cb_t key_callback = NULL;
static const gpio_num_t key_pins[KEY_MAX] = {
    BSP_KEY_UP, BSP_KEY_DOWN, BSP_KEY_LEFT, BSP_KEY_RIGHT,
    BSP_KEY_A, BSP_KEY_B, BSP_KEY_START
};

esp_err_t key_driver_init(key_cb_t cb) {
    key_callback = cb;
    for (int i = 0; i < KEY_MAX; i++) {
        gpio_set_direction(key_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(key_pins[i], GPIO_PULLUP_ONLY);
    }
    return ESP_OK;
}

void key_driver_scan_task(void *arg) {
    uint8_t last[KEY_MAX]={0}, stable[KEY_MAX]={0}, cnt[KEY_MAX]={0};
    while (1) {
        for (int i = 0; i < KEY_MAX; i++) {
            uint8_t lv = gpio_get_level(key_pins[i]);
            if (lv == last[i]) {
                if (cnt[i] < 3) cnt[i]++;
                if (cnt[i] == 3 && stable[i] != lv) {
                    stable[i] = lv;
                    if (key_callback) key_callback(i, lv == 0);
                }
            } else { cnt[i] = 0; }
            last[i] = lv;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

- [ ] **Step 3: 编译验证**

Run: `idf.py build`
Expected: 编译成功

- [ ] **Step 4: 提交**

```bash
git add main/bsp/key_driver.h main/bsp/bsp_board.c
git commit -m "feat: add GPIO key driver with software debounce"
```

---

### Task 2.2: LVGL 集成 + 主界面

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\ui\screens\main_screen.h`
- Create: `D:\code\claudecode\ESP_BSP\main\ui\screens\main_screen.c`
- Create: `D:\code\claudecode\ESP_BSP\main\ui\components\status_bar.h`
- Create: `D:\code\claudecode\ESP_BSP\main\ui\components\status_bar.c`
- Modify: `D:\code\claudecode\ESP_BSP\main\main.c`

- [ ] **Step 1: 创建 main_screen.h**

```c
#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H
#include "lvgl.h"

lv_obj_t *main_screen_create(void);
void main_screen_update_time(const char *t, const char *d);
void main_screen_update_weather(const char *w, int temp);
void main_screen_update_device_temp(float t, float h);

#endif
```

- [ ] **Step 2: 创建 main_screen.c**

```c
#include "main_screen.h"
#include "esp_log.h"
static const char *TAG = "MAIN_SCR";

static lv_obj_t *time_lbl, *date_lbl, *weather_lbl, *dev_lbl, *hint_lbl;

lv_obj_t *main_screen_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    time_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(time_lbl, "00:00");
    lv_obj_center(time_lbl);
    lv_obj_set_y(time_lbl, -40);

    date_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(date_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(date_lbl, "2026-01-01");
    lv_obj_align(date_lbl, LV_ALIGN_CENTER, 0, 10);

    weather_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(weather_lbl, lv_color_hex(0x87CEEB), 0);
    lv_label_set_text(weather_lbl, "---");
    lv_obj_align(weather_lbl, LV_ALIGN_CENTER, 0, 50);

    dev_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(dev_lbl, lv_color_hex(0x90EE90), 0);
    lv_label_set_text(dev_lbl, "Device: --C  --%");
    lv_obj_align(dev_lbl, LV_ALIGN_CENTER, 0, 80);

    hint_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0x666666), 0);
    lv_label_set_text(hint_lbl, "START > Menu");
    lv_obj_align(hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_scr_load(scr);
    return scr;
}

void main_screen_update_time(const char *t, const char *d) {
    if (time_lbl) lv_label_set_text(time_lbl, t);
    if (date_lbl) lv_label_set_text(date_lbl, d);
}
void main_screen_update_weather(const char *w, int temp) {
    if (weather_lbl) { char b[64]; snprintf(b,64,"%s  %dC",w,temp); lv_label_set_text(weather_lbl,b); }
}
void main_screen_update_device_temp(float t, float h) {
    if (dev_lbl) { char b[64]; snprintf(b,64,"Device: %.1fC  %.0f%%",t,h); lv_label_set_text(dev_lbl,b); }
}
```

- [ ] **Step 3: 创建 status_bar.h**

```c
#ifndef STATUS_BAR_H
#define STATUS_BAR_H
#include "lvgl.h"
lv_obj_t *status_bar_create(lv_obj_t *parent);
void status_bar_set_wifi(bool on);
void status_bar_set_ble(bool on);
void status_bar_set_volume(uint8_t p);
void status_bar_set_brightness(uint8_t p);
#endif
```

- [ ] **Step 4: 创建 status_bar.c**

```c
#include "status_bar.h"
static lv_obj_t *w, *b, *vl, *bl;
lv_obj_t *status_bar_create(lv_obj_t *parent) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 240, 24);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    w = lv_label_create(bar); lv_label_set_text(w, LV_SYMBOL_WIFI);    lv_obj_set_pos(w, 5, 2);
    b = lv_label_create(bar); lv_label_set_text(b, LV_SYMBOL_BLUETOOTH); lv_obj_set_pos(b, 30, 2);
    vl = lv_label_create(bar); lv_label_set_text(vl, LV_SYMBOL_VOLUME_MID " 80%"); lv_obj_set_pos(vl, 150, 2);
    bl = lv_label_create(bar); lv_label_set_text(bl, LV_SYMBOL_CHARGE " 50%");  lv_obj_set_pos(bl, 195, 2);
    return bar;
}
void status_bar_set_wifi(bool on)       { lv_label_set_text(w, on ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE); }
void status_bar_set_ble(bool on)        { lv_label_set_text(b, on ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE); }
void status_bar_set_volume(uint8_t p)   { char buf[16]; snprintf(buf,16,"%s %d%%",LV_SYMBOL_VOLUME_MID,p); lv_label_set_text(vl,buf); }
void status_bar_set_brightness(uint8_t p){ char buf[16]; snprintf(buf,16,"%s %d%%",LV_SYMBOL_CHARGE,p); lv_label_set_text(bl,buf); }
```

- [ ] **Step 5: 修改 main.c — LVGL 初始化**

```c
// main.c 完整内容
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp_board.h"
#include "st7789_driver.h"
#include "key_driver.h"
#include "lvgl.h"
#include "ui/screens/main_screen.h"

static const char *TAG = "MAIN";

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *cm) {
    st7789_set_window(a->x1, a->y1, a->x2, a->y2);
    st7789_write_ram((uint8_t*)cm, (a->x2-a->x1+1)*(a->y2-a->y1+1)*2);
    lv_disp_flush_ready(drv);
}

static void lvgl_tick_cb(void *arg) { lv_tick_inc(10); }

void app_main(void) {
    ESP_LOGI(TAG, "Starting...");
    bsp_board_init();

    lv_init();
    static lv_disp_draw_buf_t db;
    static lv_color_t *buf1 = heap_caps_malloc(240*280*sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&db, buf1, NULL, 240*280);
    static lv_disp_drv_t dd;
    lv_disp_drv_init(&dd);
    dd.flush_cb = lvgl_flush_cb;
    dd.draw_buf = &db;
    dd.hor_res = 240; dd.ver_res = 280;
    lv_disp_drv_register(&dd);

    const esp_timer_create_args_t ta = { .callback = lvgl_tick_cb, .name = "lvtick" };
    esp_timer_handle_t th;
    esp_timer_create(&ta, &th);
    esp_timer_start_periodic(th, 10000);

    main_screen_create();
    key_driver_init(NULL);
    xTaskCreatePinnedToCore(key_driver_scan_task, "key", 2048, NULL, 5, NULL, 0);

    while (1) { lv_task_handler(); vTaskDelay(pdMS_TO_TICKS(5)); }
}
```

- [ ] **Step 6: 编译烧录验证**

Run: `idf.py build && idf.py -p PORT flash monitor`
Expected: LVGL 主界面显示

- [ ] **Step 7: 提交**

```bash
git add main/ui/ main/main.c
git commit -m "feat: add LVGL with main screen and status bar"
```

---

### Task 2.3: 菜单界面 + 应用管理器

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\app\app_manager.h`
- Create: `D:\code\claudecode\ESP_BSP\main\app\app_manager.c`
- Create: `D:\code\claudecode\ESP_BSP\main\app\launcher.h`
- Create: `D:\code\claudecode\ESP_BSP\main\app\launcher.c`
- Create: `D:\code\claudecode\ESP_BSP\main\app\menu.h`
- Create: `D:\code\claudecode\ESP_BSP\main\app\menu.c`
- Create: `D:\code\claudecode\ESP_BSP\main\ui\screens\menu_screen.h`
- Create: `D:\code\claudecode\ESP_BSP\main\ui\screens\menu_screen.c`
- Modify: `D:\code\claudecode\ESP_BSP\main\main.c`

- [ ] **Step 1: 创建 app_manager.h**

```c
#ifndef APP_MANAGER_H
#define APP_MANAGER_H

typedef enum { APP_STATE_LAUNCHER, APP_STATE_MENU, APP_STATE_RUNNING } app_state_t;
typedef enum { APP_ID_NONE=-1, APP_ID_NES, APP_ID_XIAOZHI, APP_ID_PC_REMOTE, APP_ID_SETTINGS, APP_ID_ABOUT, APP_ID_MAX } app_id_t;

typedef struct {
    app_id_t id;
    const char *name;
    const char *icon;
} app_entry_t;

void app_manager_init(void);
void app_manager_set_state(app_state_t s);
app_state_t app_manager_get_state(void);
esp_err_t app_manager_launch(app_id_t id);
void app_manager_return(void);
const app_entry_t *app_manager_get_apps(void);
int app_manager_get_app_count(void);

#endif
```

- [ ] **Step 2: 创建 app_manager.c**

```c
#include "app_manager.h"
#include "launcher.h"
#include "menu.h"
#include "esp_log.h"

static const char *TAG = "APP_MGR";
static app_state_t state = APP_STATE_LAUNCHER;
static app_id_t cur_app = APP_ID_NONE;

static const app_entry_t apps[] = {
    { APP_ID_NES, "NES", "1" },
    { APP_ID_XIAOZHI, "AI", "2" },
    { APP_ID_PC_REMOTE, "PC远控", "3" },
    { APP_ID_SETTINGS, "设置", "4" },
    { APP_ID_ABOUT, "关于", "5" },
};

void app_manager_init(void) { state = APP_STATE_LAUNCHER; cur_app = APP_ID_NONE; launcher_enter(); }
void app_manager_set_state(app_state_t s) { state = s; }
app_state_t app_manager_get_state(void) { return state; }

esp_err_t app_manager_launch(app_id_t id) {
    if (id < 0 || id >= APP_ID_MAX) return ESP_ERR_INVALID_ARG;
    cur_app = id; state = APP_STATE_RUNNING;
    ESP_LOGI(TAG, "Launch: %s", apps[id].name);
    return ESP_OK;
}

void app_manager_return(void) {
    if (state == APP_STATE_RUNNING) { state = APP_STATE_MENU; menu_enter(); }
    else if (state == APP_STATE_MENU) { state = APP_STATE_LAUNCHER; launcher_enter(); }
}

const app_entry_t *app_manager_get_apps(void) { return apps; }
int app_manager_get_app_count(void) { return sizeof(apps)/sizeof(apps[0]); }
```

- [ ] **Step 3: 创建 launcher.h/c**

```c
// launcher.h
#pragma once
void launcher_enter(void);
```

```c
// launcher.c
#include "launcher.h"
#include "app_manager.h"
#include "ui/screens/main_screen.h"
#include "esp_log.h"
static const char *TAG="LAUNCHER";
void launcher_enter(void) {
    ESP_LOGI(TAG,"enter");
    main_screen_create();
    app_manager_set_state(APP_STATE_LAUNCHER);
}
```

- [ ] **Step 4: 创建 menu.h/c**

```c
// menu.h
#pragma once
void menu_enter(void);
```

```c
// menu.c
#include "menu.h"
#include "ui/screens/menu_screen.h"
#include "app_manager.h"
#include "esp_log.h"
static const char *TAG="MENU";
void menu_enter(void) {
    ESP_LOGI(TAG,"enter");
    menu_screen_create();
    app_manager_set_state(APP_STATE_MENU);
}
```

- [ ] **Step 5: 创建 menu_screen.h/c**

```c
// menu_screen.h
#pragma once
#include "lvgl.h"
lv_obj_t *menu_screen_create(void);
void menu_screen_navigate(int dx, int dy);
void menu_screen_select(void);
```

```c
// menu_screen.c
#include "menu_screen.h"
#include "app/app_manager.h"
#define GRID 3
#define SZ 70

static lv_obj_t *items[APP_ID_MAX]={NULL};
static int cx=0, cy=0, ci=0;

lv_obj_t *menu_screen_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "菜单");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    const app_entry_t *apps = app_manager_get_apps();
    int n = app_manager_get_app_count();
    for (int i = 0; i < n; i++) {
        int col = i % GRID, row = i / GRID;
        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, SZ, SZ);
        lv_obj_set_pos(btn, (240-GRID*SZ)/2+col*SZ, 50+row*(SZ+15));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x16213e), 0);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, apps[i].name);
        lv_obj_center(l);
        items[i] = btn;
    }
    lv_scr_load(scr);
    return scr;
}

void menu_screen_navigate(int dx, int dy) {
    int n = app_manager_get_app_count();
    int nx = cx+dx, ny = cy+dy, ni = ny*GRID+nx;
    if (nx<0||nx>=GRID||ny<0||ni>=n) return;
    cx=nx; cy=ny; ci=ni;
    for (int i=0;i<n;i++)
        lv_obj_set_style_bg_color(items[i], i==ci?lv_color_hex(0x0f3460):lv_color_hex(0x16213e),0);
}

void menu_screen_select(void) {
    const app_entry_t *apps = app_manager_get_apps();
    app_manager_launch(apps[ci].id);
}
```

- [ ] **Step 6: 修改 main.c — 关联按键回调**

```c
// main.c 添加
#include "app/app_manager.h"
#include "ui/screens/menu_screen.h"

static void key_handler(key_id_t key, bool pressed) {
    if (!pressed) return;
    app_state_t s = app_manager_get_state();
    if (s == APP_STATE_LAUNCHER) {
        if (key == KEY_START) { app_manager_set_state(APP_STATE_MENU); menu_enter(); }
    } else if (s == APP_STATE_MENU) {
        switch (key) {
            case KEY_UP: menu_screen_navigate(0,-1); break;
            case KEY_DOWN: menu_screen_navigate(0,1); break;
            case KEY_LEFT: menu_screen_navigate(-1,0); break;
            case KEY_RIGHT: menu_screen_navigate(1,0); break;
            case KEY_A: menu_screen_select(); break;
            case KEY_B: case KEY_START: app_manager_return(); break;
            default: break;
        }
    } else if (s == APP_STATE_RUNNING) {
        if (key == KEY_B || key == KEY_START) app_manager_return();
    }
}

// app_main 中: key_driver_init(key_handler); 替换原来的 key_driver_init(NULL);
```

- [ ] **Step 7: 编译烧录验证**

Run: `idf.py build && idf.py -p PORT flash monitor`
Expected: START 进入菜单，方向键导航，A 进入，B/START 返回

- [ ] **Step 8: 提交**

```bash
git add main/app/ main/ui/screens/menu_screen.c main/ui/screens/menu_screen.h main/main.c
git commit -m "feat: add app manager state machine with menu navigation"
```

---

## Phase 3: 传感器驱动

### Task 3.1: AHT20 温湿度

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\modules\sensor\aht20_driver.h`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\sensor\aht20_driver.c`
- Modify: `D:\code\claudecode\ESP_BSP\main\main.c`

- [ ] **Step 1: 创建 aht20_driver.h**

```c
#pragma once
#include "esp_err.h"
esp_err_t aht20_init(void);
esp_err_t aht20_read(float *temp, float *humi);
```

- [ ] **Step 2: 创建 aht20_driver.c**

```c
#include "aht20_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#define ADDR 0x38
#define I2C I2C_NUM_0
static const char *TAG="AHT20";

esp_err_t aht20_init(void) {
    uint8_t c[]={0xBE,0x08,0x00};
    esp_err_t r=i2c_master_write_to_device(I2C,ADDR,c,3,pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(10));
    return r;
}

esp_err_t aht20_read(float *temp, float *humi) {
    uint8_t t[]={0xAC,0x33,0x00};
    esp_err_t r=i2c_master_write_to_device(I2C,ADDR,t,3,pdMS_TO_TICKS(100));
    if(r) return r;
    vTaskDelay(pdMS_TO_TICKS(80));
    uint8_t b[6]={0};
    r=i2c_master_read_from_device(I2C,ADDR,b,6,pdMS_TO_TICKS(100));
    if(r||(b[0]&0x80)) return r?r:ESP_ERR_INVALID_STATE;
    uint32_t rh=((uint32_t)b[1]<<12)|((uint32_t)b[2]<<4)|(b[3]>>4);
    uint32_t rt=((uint32_t)(b[3]&0x0F)<<16)|((uint32_t)b[4]<<8)|b[5];
    *humi = (float)rh/0x100000*100.0f;
    *temp = (float)rt/0x100000*200.0f-50.0f;
    return ESP_OK;
}
```

- [ ] **Step 3: 在 main.c 中创建传感器任务**

```c
// main.c 添加
#include "modules/sensor/aht20_driver.h"
static void sensor_task(void *arg) {
    aht20_init();
    while(1){
        float t,h;
        if(aht20_read(&t,&h)==ESP_OK) main_screen_update_device_temp(t,h);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
// app_main 中添加:
xTaskCreatePinnedToCore(sensor_task,"sensor",3072,NULL,3,NULL,0);
```

- [ ] **Step 4: 编译烧录验证并提交**

```bash
idf.py build
git add main/modules/sensor/ main/main.c
git commit -m "feat: add AHT20 temperature and humidity sensor"
```

---

### Task 3.2: IMU LSM6DS3TR-C

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\modules\imu\imu_driver.h`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\imu\imu_driver.c`

- [ ] **Step 1: 创建 imu_driver.h**

```c
#pragma once
#include "esp_err.h"
typedef struct {
    float ax, ay, az;  // accel (g)
    float gx, gy, gz;  // gyro (dps)
    float temp;
} imu_data_t;
esp_err_t imu_init(void);
esp_err_t imu_read(imu_data_t *d);
```

- [ ] **Step 2: 创建 imu_driver.c**

```c
#include "imu_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#define ADDR 0x6A
#define I2C I2C_NUM_0
static const char *TAG="IMU";

esp_err_t imu_init(void) {
    uint8_t who;
    esp_err_t r=i2c_master_write_read_device(I2C,ADDR,(uint8_t[]){0x0F},1,&who,1,pdMS_TO_TICKS(100));
    if(r!=ESP_OK||who!=0x6C){ ESP_LOGE(TAG,"IMU not found 0x%02X",who); return ESP_ERR_NOT_FOUND; }
    ESP_LOGI(TAG,"IMU detected");
    i2c_master_write_to_device(I2C,ADDR,(uint8_t[]){0x10,0x40},2,pdMS_TO_TICKS(100)); // ACC 104Hz
    i2c_master_write_to_device(I2C,ADDR,(uint8_t[]){0x11,0x40},2,pdMS_TO_TICKS(100)); // GYRO 104Hz
    i2c_master_write_to_device(I2C,ADDR,(uint8_t[]){0x12,0x44},2,pdMS_TO_TICKS(100)); // IF_INC
    return ESP_OK;
}

static int16_t read_i16(uint8_t reg){
    uint8_t b[2];
    i2c_master_write_read_device(I2C,ADDR,&reg,1,b,2,pdMS_TO_TICKS(100));
    return (int16_t)(b[0]|(b[1]<<8));
}

esp_err_t imu_read(imu_data_t *d){
    d->temp=(float)read_i16(0x20)/256.0f+25.0f;
    d->gx=(float)read_i16(0x22)/70.0f; d->gy=(float)read_i16(0x24)/70.0f; d->gz=(float)read_i16(0x26)/70.0f;
    d->ax=(float)read_i16(0x28)/16384.0f; d->ay=(float)read_i16(0x2A)/16384.0f; d->az=(float)read_i16(0x2C)/16384.0f;
    return ESP_OK;
}
```

- [ ] **Step 3: 编译验证并提交**

```bash
idf.py build
git add main/modules/imu/
git commit -m "feat: add LSM6DS3TR-C IMU driver"
```

---

## Phase 4: TF 卡

### Task 4.1: SD 卡 SPI 驱动

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\sd_card.h`
- Create: `D:\code\claudecode\ESP_BSP\main\bsp\sd_card.c`
- Modify: `D:\code\claudecode\ESP_BSP\main\bsp\bsp_board.c`

- [ ] **Step 1: 创建 sd_card.h**

```c
#pragma once
#include "esp_err.h"
esp_err_t sd_card_init(void);
bool sd_card_is_mounted(void);
const char *sd_card_mount_point(void);
```

- [ ] **Step 2: 创建 sd_card.c**

```c
#include "sd_card.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

static const char *TAG="SD";
static const char *MP="/sdcard";
static bool mounted=false;

esp_err_t sd_card_init(void){
    esp_vfs_fat_sdmmc_mount_config_t mc={.format_if_mount_failed=false,.max_files=5,.allocation_unit_size=16*1024};
    sdspi_device_config_t sc=SDSPI_DEVICE_CONFIG_DEFAULT();
    sc.gpio_cs=BSP_SD_CS; sc.host_id=BSP_LCD_HOST;
    sdmmc_card_t *card;
    esp_err_t r=esp_vfs_fat_sdspi_mount(MP,&mc,&sc,&card,NULL);
    if(r!=ESP_OK){ ESP_LOGE(TAG,"SD mount fail %d",r); return r; }
    sdmmc_card_print_info(stdout,card);
    mounted=true;
    ESP_LOGI(TAG,"SD mounted at %s",MP);
    return ESP_OK;
}
bool sd_card_is_mounted(void){ return mounted; }
const char *sd_card_mount_point(void){ return MP; }
```

- [ ] **Step 3: 在 bsp_board_init 中调用**

```c
// bsp_board.c 中 bsp_board_init 末尾添加:
sd_card_init(); // 可选,失败不影响
// 并添加 #include "sd_card.h"
```

- [ ] **Step 4: 编译烧录验证并提交**

```bash
idf.py build
git add main/bsp/sd_card.c main/bsp/sd_card.h main/bsp/bsp_board.c
git commit -m "feat: add SD card SPI mode with FATFS"
```

---

## Phase 5: NES 模拟器

### Task 5.1: 集成 NES 模拟器

**Files:**
- Modify: `D:\code\claudecode\ESP_BSP\main\CMakeLists.txt`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\nes\nes_wrapper.h`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\nes\nes_wrapper.c`

- [ ] **Step 1: 添加 NES git submodule**

```bash
cd D:\code\claudecode\ESP_BSP
git submodule add https://github.com/planevina/esp32s3_nes_gamer components/nes_emulator
```

- [ ] **Step 2: 创建 nes_wrapper.h**

```c
#pragma once
#include "esp_err.h"
esp_err_t nes_start(const char *rom_path);
void nes_stop(void);
bool nes_is_running(void);
```

- [ ] **Step 3: 创建 nes_wrapper.c**

```c
#include "nes_wrapper.h"
#include "esp_log.h"
static const char *TAG="NES";
extern void nescat_start(const char*);
extern void nescat_stop(void);
esp_err_t nes_start(const char *p){ ESP_LOGI(TAG,"start %s",p); nescat_start(p); return ESP_OK; }
void nes_stop(void){ ESP_LOGI(TAG,"stop"); nescat_stop(); }
bool nes_is_running(void){ return false; }
```

- [ ] **Step 4: 注册到 app_manager**

```c
// app_manager.c: 在 app_manager_launch 中增加 NES 启动逻辑
// 当 id==APP_ID_NES 时调用 nes_start("/sdcard/roms/game.nes")
// stop 时调用 nes_stop()
```

- [ ] **Step 5: 编译验证并提交**

```bash
idf.py build
git add main/modules/nes/ components/nes_emulator
git commit -m "feat: integrate NES emulator from esp32s3_nes_gamer"
```

---

## Phase 6: BLE HID 键鼠 + 空中飞鼠

### Task 6.1: BLE HID 键盘

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\modules\pc_remote\ble_hid.h`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\pc_remote\ble_hid.c`

- [ ] **Step 1: 创建 ble_hid.h**

```c
#pragma once
#include "esp_err.h"
esp_err_t ble_hid_init(void);
esp_err_t ble_hid_send_key(uint8_t modifier, uint8_t key);
bool ble_hid_is_connected(void);
```

- [ ] **Step 2: 创建 ble_hid.c**

```c
#include "ble_hid.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hidd_api.h"

static const char *TAG="BLE_HID";
static bool connected=false;

static void cb(esp_hidd_cb_event_t e, esp_hidd_cb_param_t *p){
    if(e==ESP_HIDD_CONNECT_EVT) connected=true;
    else if(e==ESP_HIDD_DISCONNECT_EVT) connected=false;
}

esp_err_t ble_hid_init(void){
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bc=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bc));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_hidd_register_callbacks(cb));
    ESP_ERROR_CHECK(esp_hidd_init());
    esp_bt_dev_set_device_name("ESP-Handheld");
    esp_ble_gap_config_local_privacy(true);
    ESP_LOGI(TAG,"BLE HID ready");
    return ESP_OK;
}

esp_err_t ble_hid_send_key(uint8_t mod, uint8_t key){
    if(!connected) return ESP_ERR_INVALID_STATE;
    uint8_t r[8]={mod,0,key,0,0,0,0,0};
    esp_hidd_send_keyboard_value(0,r,8);
    r[0]=0; r[2]=0;
    esp_hidd_send_keyboard_value(0,r,8);
    return ESP_OK;
}

bool ble_hid_is_connected(void){ return connected; }
```

- [ ] **Step 3: 编译验证并提交**

```bash
idf.py build
git add main/modules/pc_remote/
git commit -m "feat: add BLE HID keyboard driver"
```

---

### Task 6.2: 空中飞鼠 (IMU → BLE HID Mouse)

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\modules\pc_remote\air_mouse.h`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\pc_remote\air_mouse.c`

- [ ] **Step 1: 创建 air_mouse.h**

```c
#pragma once
#include "esp_err.h"
esp_err_t air_mouse_init(void);
void air_mouse_set_sensitivity(float s);
void air_mouse_task(void *arg);
```

- [ ] **Step 2: 创建 air_mouse.c**

```c
#include "air_mouse.h"
#include "modules/imu/imu_driver.h"
#include "esp_log.h"
#include "esp_hidd_api.h"
#include <math.h>

static const char *TAG = "AIR_MOUSE";
static float sensitivity = 1.0f;
static float prev_gx = 0, prev_gy = 0;

void air_mouse_set_sensitivity(float s) { sensitivity = s; }

esp_err_t air_mouse_init(void) {
    ESP_LOGI(TAG, "Air mouse init");
    return ESP_OK;
}

void air_mouse_task(void *arg) {
    imu_data_t imu;
    while (1) {
        if (imu_read(&imu) == ESP_OK) {
            // 互补滤波平滑
            float gx = imu.gx * 0.7f + prev_gx * 0.3f;
            float gy = imu.gy * 0.7f + prev_gy * 0.3f;
            prev_gx = gx; prev_gy = gy;

            // 陀螺仪 → 鼠标位移
            // 灵敏度: 每1dps ≈ 4像素移动
            int8_t dx = (int8_t)fminf(fmaxf(gx * sensitivity * 4, -127), 127);
            int8_t dy = (int8_t)fminf(fmaxf(gy * sensitivity * 4, -127), 127);

            if (dx != 0 || dy != 0) {
                uint8_t report[5] = {0, dx, dy, 0, 0};  // buttons, x, y, wheel, pan
                esp_hidd_send_mouse_value(0, report, 5);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz 刷新
    }
}
```

- [ ] **Step 3: 提交**

```bash
git add main/modules/pc_remote/air_mouse.c
git commit -m "feat: add air mouse using IMU gyroscope"
```

---

## Phase 7: WiFi 语音传输

### Task 7.1: WiFi + UDP 语音发送

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\modules\pc_remote\wifi_audio.h`
- Create: `D:\code\claudecode\ESP_BSP\main\modules\pc_remote\wifi_audio.c`

- [ ] **Step 1: 创建 wifi_audio.h**

```c
#pragma once
#include "esp_err.h"
esp_err_t wifi_audio_init(void);
esp_err_t wifi_audio_start(const char *pc_ip, uint16_t port);
void wifi_audio_stop(void);
bool wifi_audio_is_streaming(void);
```

- [ ] **Step 2: 创建 wifi_audio.c**

```c
#include "wifi_audio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "driver/i2s_std.h"

static const char *TAG = "WIFI_AUDIO";
static bool streaming = false;
static int sock = -1;
static struct sockaddr_in dest_addr;

// WiFi station 初始化
static void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wc = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_start();
}

// I2S 麦克风配置 (ES7210)
static void i2s_mic_init(void) {
    i2s_chan_config_t ch_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&ch_cfg, NULL, NULL);
    // 详细配置: 16bit, 16kHz, mono
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BSP_MIC_BCK,
            .ws = BSP_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = BSP_MIC_DATA0,
            .invert_flags = {0},
        },
    };
    i2s_channel_init_std_mode(NULL, &std_cfg);
    i2s_channel_enable(NULL);
}

esp_err_t wifi_audio_init(void) {
    wifi_init_sta();
    i2s_mic_init();
    ESP_LOGI(TAG, "WiFi audio init done");
    return ESP_OK;
}

esp_err_t wifi_audio_start(const char *pc_ip, uint16_t port) {
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    struct hostent *he = gethostbyname(pc_ip);
    if (he) dest_addr.sin_addr = *(struct in_addr*)he->h_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { ESP_LOGE(TAG, "socket fail"); return ESP_FAIL; }

    streaming = true;
    // 创建发送任务
    xTaskCreate([](void*){
        int16_t buf[256];
        size_t read = 0;
        while (streaming) {
            i2s_channel_read(NULL, buf, sizeof(buf), &read, portMAX_DELAY);
            if (read > 0)
                sendto(sock, buf, read, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        close(sock);
        vTaskDelete(NULL);
    }, "audio_tx", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Streaming to %s:%d", pc_ip, port);
    return ESP_OK;
}

void wifi_audio_stop(void) { streaming = false; }
bool wifi_audio_is_streaming(void) { return streaming; }
```

- [ ] **Step 3: 提交**

---

### Task 7.2: PC 端接收服务

- [ ] **Step 1: 创建 tools/pc_receiver.py**

```python
import socket, pyaudio
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
sock.bind(("0.0.0.0",8888))
p=pyaudio.PyAudio()
stream=p.open(format=pyaudio.paInt16,channels=1,rate=16000,output=True,frames_per_buffer=512)
print("Listening UDP:8888")
while True: stream.write(sock.recv(4096))
```

- [ ] **Step 2: 提交**

```bash
git add tools/
git commit -m "feat: add PC receiver service for UDP voice input"
```

---

## Phase 8: 小智 AI (预留)

- [ ] 从 `D:\code\XIAOZHI_AI\xiaozhi-esp32-main\main` 拷贝核心代码到 `modules/xiaozhi/`
- [ ] 适配 audio_codec 接口到 ES8311+ES7210
- [ ] 集成 WebSocket 协议
- [ ] 注册为 APP_ID_XIAOZHI
- [ ] 编译验证并提交

---

## Phase 9: 设置模块

### Task 9.1: 设置界面

**Files:**
- Create: `D:\code\claudecode\ESP_BSP\main\ui\screens\settings_screen.h`
- Create: `D:\code\claudecode\ESP_BSP\main\ui\screens\settings_screen.c`

设置项: 音量滑块, 亮度滑块, WiFi 扫描/连接, BLE 状态, 按键自定义, 关于信息

### Task 9.2: NVS 存储

使用 ESP-IDF NVS 保存音量/亮度/WiFi配置:
```c
nvs_handle_t h;
nvs_open("settings", NVS_READWRITE, &h);
nvs_set_u8(h, "volume", vol);
nvs_set_u8(h, "bright", bright);
nvs_commit(h);
```

---

## Spec Coverage Check

| Spec 需求 | 对应任务 |
|-----------|---------|
| 屏幕点亮(ST7789) | Task 1.2 |
| 按键输入(7键) | Task 2.1 |
| LVGL 主界面(时间/日期/天气) | Task 2.2 |
| 菜单(九宫格,方向键导航) | Task 2.3 |
| 应用管理器(状态机) | Task 2.3 |
| AHT20 温湿度 | Task 3.1 |
| IMU LSM6DS3TR-C | Task 3.2 |
| TF 卡(FATFS) | Task 4.1 |
| NES 模拟器 | Task 5.1 |
| BLE 键盘/手柄 | Task 6.1 |
| 空中飞鼠 | Task 6.2 |
| WiFi 语音传输 | Task 7.1 |
| PC 端接收服务 | Task 7.2 |
| 小智 AI | Phase 8 |
| 设置(音量/亮度/WiFi/BLE) | Phase 9 |
