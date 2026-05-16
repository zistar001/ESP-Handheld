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
