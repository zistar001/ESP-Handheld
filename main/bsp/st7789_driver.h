#ifndef ST7789_DRIVER_H
#define ST7789_DRIVER_H

#include "esp_err.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST7789_WIDTH   240
#define ST7789_HEIGHT  280

esp_err_t st7789_init(spi_device_handle_t spi);
esp_err_t st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
esp_err_t st7789_write_ram(uint8_t *data, size_t len);
esp_err_t st7789_sleep_in(void);
esp_err_t st7789_sleep_out(void);
esp_lcd_panel_handle_t st7789_get_panel(void);

/* 获取SPI总线host设备（供SD卡等外设共享SPI总线） */
spi_host_device_t st7789_get_spi_host(void);

/* NES渲染时获取/释放SPI总线独占权（防止与LVGL冲突） */
void st7789_bus_acquire(void);
void st7789_bus_release(void);

/* SPI总线互斥锁 — LCD和SD卡共享SPI2_HOST时使用 */
void st7789_spi_lock(void);
void st7789_spi_unlock(void);

#ifdef __cplusplus
}
#endif

#endif
