/* ESP-BSP panel handle display driver — uses BSP's existing ST7789 via esp_lcd */
#pragma once
#include <driver/gpio.h>
#include "bsp/st7789_driver.h"
#include "esp_lcd_panel_ops.h"

static uint16_t s_bsp_buf[LCD_BUFFER_LENGTH];
static int s_bsp_x0, s_bsp_y0, s_bsp_w, s_bsp_h;

static void spi_init(void) { /* BSP already initialized SPI2_HOST */ }
static void spi_deinit(void) { /* BSP manages SPI */ }

static inline uint16_t *lcd_get_buffer(size_t length)
{
    (void)length;
    return s_bsp_buf;
}

static inline void lcd_send_buffer(uint16_t *buffer, size_t length)
{
    esp_lcd_panel_handle_t panel = st7789_get_panel();
    if (!panel) return;
    int h = s_bsp_w > 0 ? (int)length / s_bsp_w : 1;
    if (h < 1) h = 1;
    esp_lcd_panel_draw_bitmap(panel, s_bsp_x0, s_bsp_y0, s_bsp_x0 + s_bsp_w, s_bsp_y0 + h, (const void *)buffer);
}

static void lcd_set_window(int left, int top, int width, int height)
{
    s_bsp_x0 = left; s_bsp_y0 = top;
    s_bsp_w = width > 0 ? width : RG_SCREEN_WIDTH;
    s_bsp_h = height > 0 ? height : RG_SCREEN_HEIGHT;
}

static void lcd_init(void)
{
    esp_lcd_panel_handle_t panel = st7789_get_panel();
    if (panel) esp_lcd_panel_disp_on_off(panel, true);
    lcd_set_window(0, 0, RG_SCREEN_WIDTH, RG_SCREEN_HEIGHT);
}

static void lcd_deinit(void) {}

static void lcd_sync(void) {}

static void lcd_set_backlight(float percent)
{
    extern void bsp_lcd_backlight_set(int duty);
    bsp_lcd_backlight_set((int)(percent * 2.55f));
}

static void lcd_set_rotation(int rotation) { (void)rotation; }
