#include "st7789_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ST7789";

static esp_lcd_panel_io_handle_t panel_io = NULL;
static esp_lcd_panel_handle_t panel = NULL;
static bool s_initialized = false;
static SemaphoreHandle_t s_spi_mutex = NULL;

esp_err_t st7789_init(spi_device_handle_t spi) {
    (void)spi;
    ESP_LOGI(TAG, "st7789_init (s_initialized=%d)", s_initialized);

    if (!s_spi_mutex) s_spi_mutex = xSemaphoreCreateMutex();

    if (s_initialized) {
        if (panel)    { esp_lcd_panel_del(panel);       panel = NULL; }
        if (panel_io) { esp_lcd_panel_io_del(panel_io);  panel_io = NULL; }
        spi_bus_free(BSP_LCD_HOST);
        s_initialized = false;
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    /* === SPI bus — 完全匹配 XiaoZhi InitializeSpi() === */
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = BSP_LCD_MOSI;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = BSP_LCD_CLK;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = ST7789_WIDTH * ST7789_HEIGHT * sizeof(uint16_t);
    esp_err_t ret = spi_bus_initialize(BSP_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "SPI bus already exists, skipping");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* === Panel IO — 完全匹配 XiaoZhi InitializeSt7789Display() === */
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = BSP_LCD_CS;
    io_config.dc_gpio_num = BSP_LCD_DC;
    io_config.spi_mode = 3;
    io_config.pclk_hz = 60 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(BSP_LCD_HOST, &io_config, &panel_io));

    ESP_LOGI(TAG, "Install ST7789 driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = BSP_LCD_RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* ----------------------------------------------------------------
     * ST7789V 增强初始化 — ESP-IDF 默认 init 不包含这些寄存器，
     * 缺少它们可能导致 VCOM/VRH 电压不准，产生黑色横条纹。
     * ---------------------------------------------------------------- */
    vTaskDelay(pdMS_TO_TICKS(20));

    /* 电压/电源控制 */
    esp_lcd_panel_io_tx_param(panel_io, 0xC2, (uint8_t[]){0x0D, 0x02}, 2);  /* VDV_VRH_EN */
    esp_lcd_panel_io_tx_param(panel_io, 0xC3, (uint8_t[]){0x0C}, 1);        /* VRH */
    esp_lcd_panel_io_tx_param(panel_io, 0xC4, (uint8_t[]){0x0D}, 1);        /* VDV */
    esp_lcd_panel_io_tx_param(panel_io, 0xBB, (uint8_t[]){0x1F}, 1);        /* VCOM */
    esp_lcd_panel_io_tx_param(panel_io, 0xC0, (uint8_t[]){0x2C}, 1);        /* LCMCTRL */
    esp_lcd_panel_io_tx_param(panel_io, 0xD0, (uint8_t[]){0xA4, 0xA1}, 2);  /* PWCTRL1 */
    /* 注：0xB0 (RAMCTRL) 由 ESP-IDF panel_st7789_init 设置，此处不覆盖 */

    /* 正极伽玛曲线 */
    esp_lcd_panel_io_tx_param(panel_io, 0xE0, (uint8_t[]){
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23
    }, 14);
    /* 负极伽玛曲线 */
    esp_lcd_panel_io_tx_param(panel_io, 0xE1, (uint8_t[]){
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23
    }, 14);

    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    /* ST7789V: 240x320 native → 240x280 visible. Gap(0,20) = start at row 20 */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 0, 20));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    s_initialized = true;
    ESP_LOGI(TAG, "ST7789V ready (240x280, 80MHz, init+voltage+gamma)");
    return ESP_OK;
}

/* 互斥锁 */
void st7789_spi_lock(void)   { if (s_spi_mutex) xSemaphoreTake(s_spi_mutex, portMAX_DELAY); }
void st7789_spi_unlock(void) { if (s_spi_mutex) xSemaphoreGive(s_spi_mutex); }
void st7789_bus_acquire(void) {}
void st7789_bus_release(void) {}

/* NES 渲染 stub */
esp_err_t st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) { (void)x1;(void)y1;(void)x2;(void)y2; return ESP_OK; }
esp_err_t st7789_write_ram(uint8_t *data, size_t len) { (void)data;(void)len; return ESP_OK; }
esp_err_t st7789_sleep_in(void)  { if (panel) return esp_lcd_panel_disp_on_off(panel, false); return ESP_ERR_INVALID_STATE; }
esp_err_t st7789_sleep_out(void) { if (panel) return esp_lcd_panel_disp_on_off(panel, true);  return ESP_ERR_INVALID_STATE; }
esp_lcd_panel_handle_t st7789_get_panel(void) { return panel; }
spi_host_device_t st7789_get_spi_host(void) { return BSP_LCD_HOST; }
