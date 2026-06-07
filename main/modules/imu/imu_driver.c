#include "imu_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU";
#define ADDR 0x6A

static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t imu_init(void) {
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

    uint8_t who;
    uint8_t reg = 0x0F;
    r = i2c_master_transmit_receive(s_dev, &reg, 1, &who, 1, 100);
    if (r != ESP_OK || who != 0x6C) {
        ESP_LOGE(TAG, "IMU not found 0x%02X", who);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "IMU detected");

    uint8_t acc_cfg[] = {0x10, 0x40};
    i2c_master_transmit(s_dev, acc_cfg, 2, 100);  // ACC 104Hz

    uint8_t gyro_cfg[] = {0x11, 0x40};
    i2c_master_transmit(s_dev, gyro_cfg, 2, 100); // GYRO 104Hz

    uint8_t if_cfg[] = {0x12, 0x44};
    i2c_master_transmit(s_dev, if_cfg, 2, 100);   // IF_INC

    return ESP_OK;
}

static int16_t read_i16(uint8_t reg) {
    uint8_t b[2];
    i2c_master_transmit_receive(s_dev, &reg, 1, b, 2, 100);
    return (int16_t)(b[0] | (b[1] << 8));
}

esp_err_t imu_read(imu_data_t *d) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    d->temp = (float)read_i16(0x20) / 256.0f + 25.0f;
    d->gx = (float)read_i16(0x22) / 70.0f;
    d->gy = (float)read_i16(0x24) / 70.0f;
    d->gz = (float)read_i16(0x26) / 70.0f;
    d->ax = (float)read_i16(0x28) / 16384.0f;
    d->ay = (float)read_i16(0x2A) / 16384.0f;
    d->az = (float)read_i16(0x2C) / 16384.0f;
    return ESP_OK;
}
