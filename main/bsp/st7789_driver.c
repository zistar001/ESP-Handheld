#include "st7789_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "ST7789";

static esp_lcd_panel_io_handle_t panel_io = NULL;
static esp_lcd_panel_handle_t panel = NULL;
static bool s_initialized = false;

/* 互斥锁 — 保护NES渲染与LVGL并发访问 */
static SemaphoreHandle_t s_spi_mutex = NULL;

esp_err_t st7789_init(spi_device_handle_t spi) {
    ESP_LOGI(TAG, "st7789_init (s_initialized=%d)", s_initialized);

    if (!s_spi_mutex) {
        s_spi_mutex = xSemaphoreCreateMutex();
    }

    /* 已初始化 → 先清理 */
    if (s_initialized) {
        ESP_LOGI(TAG, "Re-init: cleaning up...");
        if (panel)    { esp_lcd_panel_del(panel);       panel = NULL; }
        if (panel_io) { esp_lcd_panel_io_del(panel_io);  panel_io = NULL; }
        spi_bus_free(BSP_LCD_HOST);
        s_initialized = false;
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    /* 创建SPI总线 — LCD专用，不需要MISO */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = BSP_LCD_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = ST7789_WIDTH * ST7789_HEIGHT * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(BSP_LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        /* 总线已存在（SD卡已初始化），忽略 — retro-go方案 */
        ESP_LOGI(TAG, "SPI bus already initialized, adding LCD device");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 创建panel IO */
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BSP_LCD_CS,
        .dc_gpio_num = BSP_LCD_DC,
        .spi_mode = 3,
        .pclk_hz = 60 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(BSP_LCD_HOST, &io_config, &panel_io));

    /* 创建ST7789面板 */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    esp_lcd_panel_set_gap(panel, 0, 20);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    s_initialized = true;
    ESP_LOGI(TAG, "ST7789V ready (240x280, 60MHz)");
    return ESP_OK;
}

/* ================================================================
 * 互斥锁
 * ================================================================ */

void st7789_spi_lock(void) {
    if (s_spi_mutex) xSemaphoreTake(s_spi_mutex, portMAX_DELAY);
}

void st7789_spi_unlock(void) {
    if (s_spi_mutex) xSemaphoreGive(s_spi_mutex);
}

void st7789_bus_acquire(void) {}
void st7789_bus_release(void) {}

/* ================================================================
 * NES渲染 — stubs（nes_port.cpp 直接用 panel API）
 * ================================================================ */

esp_err_t st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    (void)x1; (void)y1; (void)x2; (void)y2;
    return ESP_OK;
}

esp_err_t st7789_write_ram(uint8_t *data, size_t len) {
    (void)data; (void)len;
    return ESP_OK;
}

esp_err_t st7789_sleep_in(void) {
    if (panel) return esp_lcd_panel_disp_on_off(panel, false);
    return ESP_ERR_INVALID_STATE;
}

esp_err_t st7789_sleep_out(void) {
    if (panel) return esp_lcd_panel_disp_on_off(panel, true);
    return ESP_ERR_INVALID_STATE;
}

esp_lcd_panel_handle_t st7789_get_panel(void) { return panel; }
spi_host_device_t st7789_get_spi_host(void)   { return BSP_LCD_HOST; }
