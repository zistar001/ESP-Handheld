#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

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
