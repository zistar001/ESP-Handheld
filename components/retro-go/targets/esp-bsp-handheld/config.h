// ESP32-S3 Handheld (ESP_BSP) — standalone retro-go target config
// This file is used when building the retro-go firmware (ota_0 partition).
#pragma once

#define RG_TARGET_NAME             "ESP32-S3 Handheld"

// ── Storage: SD card on SPI2_HOST (shared with LCD) ──
#define RG_STORAGE_DRIVER           1
#define RG_STORAGE_ROOT             "/sdcard"
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT

// ── Video: ST7789 240x280 via direct SPI driver ──
#define RG_SCREEN_DRIVER            0       // 0 = ILI9341/ST7789 direct SPI
#define RG_SCREEN_WIDTH             240
#define RG_SCREEN_HEIGHT            280
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_RGB_BGR           0       // 0=RGB, 1=BGR
#define RG_SCREEN_ROTATION          0       // 0-7 rotation/reflection

// ── LCD SPI pins (SPI2_HOST, shared with SD card) ──
// MISO must match SD's MISO so the SPI bus is initialized with all 3 lines.
#define RG_GPIO_LCD_MISO            GPIO_NUM_8
#define RG_GPIO_LCD_MOSI            GPIO_NUM_13
#define RG_GPIO_LCD_CLK             GPIO_NUM_12
#define RG_GPIO_LCD_CS              GPIO_NUM_11
#define RG_GPIO_LCD_DC              GPIO_NUM_10
#define RG_GPIO_LCD_RST             GPIO_NUM_14
#define RG_GPIO_LCD_BCKL            GPIO_NUM_9
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M

// ── ST7789 init sequence (overrides ili9341.h defaults) ──
#define RG_SCREEN_INIT()                                                   \
    ILI9341_CMD(0x01, 0x00);       /* SWRESET — must re-init from reset */ \
    rg_task_delay(150);                                                    \
    ILI9341_CMD(0x11, 0x00);       /* SLPOUT */                            \
    rg_task_delay(200);                                                    \
    ILI9341_CMD(0x36, 0x00);       /* MADCTL: RGB (bit3=0), no mirror */  \
    ILI9341_CMD(0x3A, 0x05);       /* COLMOD: 16bit RGB565, MCU mode */   \
    ILI9341_CMD(0x29, 0x00);       /* DISPON */                            \
    ILI9341_CMD(0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33); /* PORCH */         \
    ILI9341_CMD(0xB7, 0x35);       /* GATE */                              \
    ILI9341_CMD(0xBB, 0x1F);       /* VCOM */                              \
    ILI9341_CMD(0xC0, 0x2C);       /* LCMCTRL */                           \
    ILI9341_CMD(0xC2, 0x01);       /* VDV_VRH_EN */                        \
    ILI9341_CMD(0xC3, 0x0C);       /* VRH */                               \
    ILI9341_CMD(0xC4, 0x0D);       /* VDV */                               \
    ILI9341_CMD(0xD0, 0xA4, 0xA1); /* PWCTRL1 */                           \
    ILI9341_CMD(0x21, 0x00);       /* INVON */                             \
    ILI9341_CMD(0x13, 0x00);       /* NORON */                             \
    rg_task_delay(100);

// ── Audio: I2S DAC (ES8311) ──
#define RG_AUDIO_USE_INT_DAC        0
#define RG_AUDIO_USE_EXT_DAC        1
#define RG_GPIO_SND_I2S_MCLK        GPIO_NUM_40
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_39
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_45
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_48
#define RG_GPIO_SND_AMP_ENABLE      GPIO_NUM_46

// ── Input: 7 physical keys via GPIO ──
#define RG_GAMEPAD_DRIVER           1
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,    .num = GPIO_NUM_5,  .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,  .num = GPIO_NUM_6,  .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,  .num = GPIO_NUM_4,  .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT, .num = GPIO_NUM_7,  .pullup = 1, .level = 0},\
    {RG_KEY_A,     .num = GPIO_NUM_16, .pullup = 1, .level = 0},\
    {RG_KEY_B,     .num = GPIO_NUM_17, .pullup = 1, .level = 0},\
    {RG_KEY_START, .num = GPIO_NUM_15, .pullup = 1, .level = 0},\
}
#define RG_GAMEPAD_HAS_MENU_BTN     0
#define RG_GAMEPAD_HAS_OPTION_BTN   0
// No built-in SELECT — handled by START+A combo in rg_system_tick_hook

// ── Battery: not used (BSP handles power) ──
#define RG_BATTERY_DRIVER           0
#define RG_GPIO_LED                 (-1)

// ── SD card GPIOs (shared SPI2_HOST with LCD) ──
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_8
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_13
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_12
#define RG_GPIO_SDSPI_CS            GPIO_NUM_18

// ── Recovery ──
#define RG_RECOVERY_BTN             0

// ── ST7789 has 240x320 memory (our glass shows 240x280) ──
// Full-memory clear prevents ghosting at screen edges.
#define RG_SCREEN_FULL_HEIGHT       320
