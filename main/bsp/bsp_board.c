#include "bsp_board.h"
#include "st7789_driver.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "BSP";
static spi_device_handle_t lcd_spi = NULL;
spi_device_handle_t bsp_get_lcd_spi(void) { return lcd_spi; }

esp_err_t bsp_lcd_backlight_set(uint8_t brightness_percent) {
    if (brightness_percent > 100) brightness_percent = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (brightness_percent * 8192) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    return ESP_OK;
}

esp_err_t bsp_lcd_init(void) {
    ESP_LOGI(TAG, "LCD init...");
    gpio_set_direction(BSP_LCD_BL, GPIO_MODE_OUTPUT);
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_ch = {
        .gpio_num = BSP_LCD_BL, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0, .timer_sel = LEDC_TIMER_0,
        .duty = 0, .hpoint = 0
    };
    ledc_channel_config(&ledc_ch);
    bsp_lcd_backlight_set(50);

    gpio_set_direction(BSP_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BSP_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_LCD_MOSI, .miso_io_num = -1,
        .sclk_io_num = BSP_LCD_CLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 280 * 2 + 8
    };
    spi_bus_initialize(BSP_LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 60 * 1000 * 1000,
        .mode = 0, .spics_io_num = BSP_LCD_CS,
        .queue_size = 7, .flags = SPI_DEVICE_NO_DUMMY
    };
    spi_bus_add_device(BSP_LCD_HOST, &dev_cfg, &lcd_spi);

    st7789_init(lcd_spi);
    ESP_LOGI(TAG, "LCD init done");
    return ESP_OK;
}

esp_err_t bsp_i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_I2C_SDA, .scl_io_num = BSP_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BSP_I2C_CLK
    };
    return i2c_param_config(I2C_NUM_0, &conf) |
           i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t bsp_board_init(void) {
    ESP_LOGI(TAG, "BSP init");
    bsp_lcd_init();
    bsp_i2c_init();
    return ESP_OK;
}
