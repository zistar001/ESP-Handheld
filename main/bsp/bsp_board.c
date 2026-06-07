#include "bsp_board.h"
#include "st7789_driver.h"
#include "sd_card.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"

static const char *TAG = "BSP";
static i2c_master_bus_handle_t s_i2c_bus = NULL;

esp_err_t bsp_lcd_backlight_set(uint8_t brightness_percent) {
    if (brightness_percent > 100) brightness_percent = 100;
    /* 13-bit duty: 0~8191, use 8191 as max */
    uint32_t duty = (brightness_percent * 8191) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    return ESP_OK;
}

esp_err_t bsp_lcd_init(void) {
    ESP_LOGI(TAG, "LCD init...");

    /* Configure backlight PWM */
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
    bsp_lcd_backlight_set(50);  /* 50% brightness initially */

    /* Initialize ST7789 via ESP-IDF panel API */
    st7789_init(NULL);  /* NULL = let st7789_init create its own SPI */

    ESP_LOGI(TAG, "LCD init done");
    return ESP_OK;
}

esp_err_t bsp_i2c_init(void) {
    ESP_LOGI(TAG, "I2C init (new driver)...");
    ESP_LOGI(TAG, "SDA=%d, SCL=%d", BSP_I2C_SDA, BSP_I2C_SCL);
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_LOGI(TAG, "Creating I2C bus...");
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C init OK, handle=%p", s_i2c_bus);
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_get_i2c_bus(void) {
    return s_i2c_bus;
}

esp_err_t bsp_board_init(void) {
    ESP_LOGI(TAG, "BSP init");
    bsp_lcd_init();
    bsp_i2c_init();
    // 延迟SD卡初始化，避免SPI总线冲突
    // sd_card_init();
    return ESP_OK;
}

/* ---- Key Driver ---- */
#include "key_driver.h"

static key_cb_t key_callback = NULL;
static const gpio_num_t key_pins[KEY_MAX] = {
    BSP_KEY_UP, BSP_KEY_DOWN, BSP_KEY_LEFT, BSP_KEY_RIGHT,
    BSP_KEY_A, BSP_KEY_B, BSP_KEY_START
};

esp_err_t key_driver_init(key_cb_t cb) {
    key_callback = cb;
    for (int i = 0; i < KEY_MAX; i++) {
        gpio_set_direction(key_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(key_pins[i], GPIO_PULLUP_ONLY);
    }
    return ESP_OK;
}

void key_driver_scan_task(void *arg) {
    uint8_t last[KEY_MAX]={0}, stable[KEY_MAX]={0}, cnt[KEY_MAX]={0};
    while (1) {
        for (int i = 0; i < KEY_MAX; i++) {
            uint8_t lv = gpio_get_level(key_pins[i]);
            if (lv == last[i]) {
                if (cnt[i] < 3) cnt[i]++;
                if (cnt[i] == 3 && stable[i] != lv) {
                    stable[i] = lv;
                    if (key_callback) key_callback(i, lv == 0);
                }
            } else { cnt[i] = 0; }
            last[i] = lv;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
