#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== NEW PCB (Rev 2) GPIO Map =====
 * Source: IO2.txt
 * LCD + SD now use SEPARATE SPI buses (no longer shared)
 */

/* LCD — ST7789, SPI2_HOST */
#define BSP_LCD_BL      GPIO_NUM_43
#define BSP_LCD_DC      GPIO_NUM_44
#define BSP_LCD_CS      GPIO_NUM_42
#define BSP_LCD_CLK     GPIO_NUM_21
#define BSP_LCD_MOSI    GPIO_NUM_14
#define BSP_LCD_RST     GPIO_NUM_13
#define BSP_LCD_HOST    SPI2_HOST

/* SD 卡 — 独立 SPI3_HOST（不再与LCD共享总线） */
#define BSP_SD_HOST     SPI3_HOST
#define BSP_SD_CLK      GPIO_NUM_40
#define BSP_SD_MOSI     GPIO_NUM_39
#define BSP_SD_MISO     GPIO_NUM_41
#define BSP_SD_CS       GPIO_NUM_38

/* I2C — 传感器总线 (IMU LSM6DS3TR-C + AHT20 共用) */
#define BSP_I2C_SCL     GPIO_NUM_2
#define BSP_I2C_SDA     GPIO_NUM_1
#define BSP_I2C_CLK     100000

/* 数字麦克风 MSM261 — I2S0 RX */
#define BSP_MIC_BCLK    GPIO_NUM_11
#define BSP_MIC_WS      GPIO_NUM_12
#define BSP_MIC_DATA    GPIO_NUM_10
#define BSP_MIC_HOST    I2S_NUM_0

/* 数字功放 MAX98357 — I2S1 TX */
#define BSP_AMP_DIN     GPIO_NUM_3
#define BSP_AMP_BCLK    GPIO_NUM_46
#define BSP_AMP_LRC     GPIO_NUM_9
#define BSP_AMP_HOST    I2S_NUM_1

/* 按键 (7个物理按键 + BOOT) */
#define BSP_KEY_UP      GPIO_NUM_16
#define BSP_KEY_DOWN    GPIO_NUM_15
#define BSP_KEY_LEFT    GPIO_NUM_17
#define BSP_KEY_RIGHT   GPIO_NUM_7
#define BSP_KEY_A       GPIO_NUM_4
#define BSP_KEY_B       GPIO_NUM_5
#define BSP_KEY_START   GPIO_NUM_6
#define BSP_KEY_BOOT    GPIO_NUM_0

/* 电池 — 无ADC检测（IO3被AMP占用），只有充电状态GPIO需用户补充 */
/* #define BSP_BAT_CHRG  GPIO_NUM_xx */
/* #define BSP_BAT_DONE  GPIO_NUM_xx */

/* LED 指示 */
#define BSP_LED_GPIO    GPIO_NUM_18

esp_err_t bsp_board_init(void);
esp_err_t bsp_lcd_init(void);
esp_err_t bsp_lcd_backlight_set(uint8_t brightness_percent);
esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_get_i2c_bus(void);

#ifdef __cplusplus
}
#endif
#endif
