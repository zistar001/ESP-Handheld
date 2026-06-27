#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Display configuration */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1  /* ST7789需要字节交换 */

/* Display size (ST7789) */
#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 280
#define LV_DPI 130

/* Memory configuration */
#define LV_MEM_CUSTOM 0
#if !LV_MEM_CUSTOM
#define LV_MEM_SIZE (64 * 1024)
#endif
#define LV_MEM_BUF_MAX_NUM 16

/* Tick */
#define LV_TICK_CUSTOM 0

/* Features */
#define LV_USE_LOG 0
#define LV_USE_OBJ_REALIGN 0
#define LV_USE_ANIMATION 1
#define LV_USE_SHADOW 0
#define LV_USE_GROUP 1
#define LV_USE_MSG 0

/* Widgets we actually use */
#define LV_USE_BTN 1
#define LV_USE_LABEL 1
#define LV_USE_SLIDER 1
#define LV_USE_DROPDOWN 1
#define LV_USE_ROLLER 0
#define LV_USE_CHART 0
#define LV_USE_TABLE 0
#define LV_USE_CHECKBOX 0
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_CANVAS 1
#define LV_USE_IMGBTN 0
#define LV_USE_IMG 1
#define LV_USE_LINE 0

/* Theme */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* Fonts */
#define LV_FONT_MONTSERRAT_8 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_SIMSUN_16_CJK 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Symbols */
#define LV_USE_SYMBOL_BATTERY_FULL 1
#define LV_USE_SYMBOL_BLUETOOTH 1
#define LV_USE_SYMBOL_WIFI 1
#define LV_USE_SYMBOL_VOLUME_MID 1
#define LV_USE_SYMBOL_CHARGE 1
#define LV_USE_SYMBOL_CLOSE 1

/* Animations */
#define LV_USE_DEMO_WIDGETS 0

/* Built-in drivers */
#define LV_USE_FS_FATFS 0

/* Enable GPU (none on ESP32-S3) */
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG 0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_SWM341_DMA2D 0
#define LV_USE_GPU_ARM2D 0

/* Assert */
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MEM 0
#define LV_USE_ASSERT_STR 0
#define LV_USE_ASSERT_OBJ 0

#endif /* LV_CONF_H */
