#include "aht20_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AHT20";
#define ADDR 0x38

static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t aht20_init(void) {
    i2c_master_bus_handle_t bus = bsp_get_i2c_bus();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADDR,
        .scl_speed_hz = 100000,
    };
    esp_err_t r = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %s", esp_err_to_name(r));
        return r;
    }

    uint8_t c[] = {0xBE, 0x08, 0x00};
    r = i2c_master_transmit(s_dev, c, 3, 100);
    vTaskDelay(pdMS_TO_TICKS(10));

    if (r == ESP_OK) {
        ESP_LOGI(TAG, "AHT20 initialized");
    }
    return r;
}

esp_err_t aht20_read(float *temp, float *humi) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    uint8_t cmd[] = {0xAC, 0x33, 0x00};
    esp_err_t r = i2c_master_transmit(s_dev, cmd, 3, 100);
    if (r != ESP_OK) return r;

    vTaskDelay(pdMS_TO_TICKS(80));

    uint8_t d[6];
    r = i2c_master_receive(s_dev, d, 6, 100);
    if (r != ESP_OK) return r;

    if (d[0] & 0x80) return ESP_ERR_NOT_FINISHED;

    uint32_t rh = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
    uint32_t rt = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

    *humi = (float)rh / 0x100000 * 100.0f;
    *temp = (float)rt / 0x100000 * 200.0f - 50.0f;

    return ESP_OK;
}
