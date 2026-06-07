#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

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

/* SD卡 (SPI模式，与LCD共享SPI2_HOST) */
#define BSP_SD_CS       GPIO_NUM_18  /* DAT3飞线到GPIO18 */

/* I2C — 外部板级传感器 */
#define BSP_I2C_SCL     GPIO_NUM_1
#define BSP_I2C_SDA     GPIO_NUM_2
#define BSP_I2C_CLK     100000

/* Audio I2S — 模组内部 ES8311+ES7210 共享总线 */
#define BSP_AUDIO_MCLK    GPIO_NUM_40
#define BSP_AUDIO_BCLK    GPIO_NUM_39
#define BSP_AUDIO_WS      GPIO_NUM_45
#define BSP_AUDIO_DOUT    GPIO_NUM_48   /* TX → ES8311 DIN */
#define BSP_AUDIO_DIN     GPIO_NUM_21   /* RX ← ES7210 DOUT */
#define BSP_AUDIO_PA_PIN  GPIO_NUM_46

/* Audio I2C — 模组内部音频编解码器控制（与传感器 I2C 独立） */
#define BSP_CODEC_SDA     GPIO_NUM_38
#define BSP_CODEC_SCL     GPIO_NUM_47

/* 电池/ADC */
#define BSP_BAT_ADC_CH  ADC_CHANNEL_2   /* GPIO 3 — 电池分压检测 */
#define BSP_BAT_CHRG    GPIO_NUM_41     /* 充电状态（低=充电中） */
#define BSP_BAT_DONE    GPIO_NUM_42     /* 充满指示（高=充满） */

/* 按键 */
#define BSP_KEY_UP      GPIO_NUM_5
#define BSP_KEY_DOWN    GPIO_NUM_6
#define BSP_KEY_LEFT    GPIO_NUM_4
#define BSP_KEY_RIGHT   GPIO_NUM_7
#define BSP_KEY_A       GPIO_NUM_17
#define BSP_KEY_B       GPIO_NUM_16
#define BSP_KEY_START   GPIO_NUM_15

esp_err_t bsp_board_init(void);
esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_backlight_set(uint8_t brightness_percent);
esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_get_i2c_bus(void);

#ifdef __cplusplus
}
#endif
#endif
