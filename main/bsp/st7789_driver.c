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
