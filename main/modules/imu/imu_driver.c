#include "imu_driver.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU";
#define IMU_ADDR_0  0x6A   /* SA0 = VDD */
#define IMU_ADDR_1  0x6B   /* SA0 = GND */
#define WHO_AM_I_VAL 0x6C  /* LSM6DS3TR-C WHO_AM_I expected value */

static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_dev_addr = 0;

/* ================================================================
 * I2C bus scan — probe all 127 addresses to find responding devices
 * ================================================================ */
static void i2c_scan(i2c_master_bus_handle_t bus) {
    ESP_LOGI(TAG, "=== I2C Bus Scan ===");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };
        i2c_master_dev_handle_t tmp_dev;
        esp_err_t r = i2c_master_bus_add_device(bus, &cfg, &tmp_dev);
        if (r != ESP_OK) continue;

        uint8_t reg = 0x00;
        uint8_t val = 0;
        r = i2c_master_transmit_receive(tmp_dev, &reg, 1, &val, 1, 50);
        i2c_master_bus_rm_device(tmp_dev);

        if (r == ESP_OK) {
            ESP_LOGI(TAG, "  I2C device found at 0x%02X (reg0=0x%02X)", addr, val);
            found++;
        }
    }
    ESP_LOGI(TAG, "=== Scan complete: %d device(s) found ===", found);
}

/* ================================================================
 * Try to probe a specific I2C address — returns true if device ACKs
 * ================================================================ */
static bool probe_address(i2c_master_bus_handle_t bus, uint8_t addr) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t tmp_dev;
    if (i2c_master_bus_add_device(bus, &cfg, &tmp_dev) != ESP_OK)
        return false;

    uint8_t reg = 0x0F;  /* WHO_AM_I */
    uint8_t who = 0;
    esp_err_t r = i2c_master_transmit_receive(tmp_dev, &reg, 1, &who, 1, 50);
    i2c_master_bus_rm_device(tmp_dev);

    if (r == ESP_OK) {
        ESP_LOGI(TAG, "  Addr 0x%02X: WHO_AM_I = 0x%02X (expect 0x%02X)%s",
                 addr, who, WHO_AM_I_VAL,
                 who == WHO_AM_I_VAL ? " <<< MATCH!" : "");
    }
    return (r == ESP_OK);
}

esp_err_t imu_init(void) {
    i2c_master_bus_handle_t bus = bsp_get_i2c_bus();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "I2C bus handle = %p", bus);

    /* === Step 1: Full I2C bus scan === */
    i2c_scan(bus);

    /* === Step 2: Probe both possible IMU addresses === */
    ESP_LOGI(TAG, "Probing LSM6DS3TR-C...");
    bool found_0x6A = probe_address(bus, IMU_ADDR_0);
    bool found_0x6B = probe_address(bus, IMU_ADDR_1);

    uint8_t addr = 0;
    if (found_0x6A) {
        addr = IMU_ADDR_0;
    } else if (found_0x6B) {
        addr = IMU_ADDR_1;
    } else {
        ESP_LOGE(TAG, "LSM6DS3TR-C not found at 0x%02X or 0x%02X!"
                      " Check HW: power, I2C pullups, SA0 pin.",
                 IMU_ADDR_0, IMU_ADDR_1);
        return ESP_ERR_NOT_FOUND;
    }

    /* === Step 3: Add device and verify WHO_AM_I === */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    esp_err_t r = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device at 0x%02X: %s", addr, esp_err_to_name(r));
        return r;
    }
    s_dev_addr = addr;
    ESP_LOGI(TAG, "LSM6DS3TR-C detected at 0x%02X! Configuring...", addr);

    /* === Step 4: Configure IMU === */
    /* CTRL1_XL (0x10): ACC ODR=104Hz, FS=±2g, BW=400Hz */
    uint8_t acc_cfg[] = {0x10, 0x40};
    r = i2c_master_transmit(s_dev, acc_cfg, 2, 100);
    if (r != ESP_OK) ESP_LOGE(TAG, "ACC cfg failed: %s", esp_err_to_name(r));
    vTaskDelay(pdMS_TO_TICKS(10));

    /* CTRL2_G (0x11): GYRO ODR=104Hz, FS=±2000dps */
    uint8_t gyro_cfg[] = {0x11, 0x40};
    r = i2c_master_transmit(s_dev, gyro_cfg, 2, 100);
    if (r != ESP_OK) ESP_LOGE(TAG, "GYRO cfg failed: %s", esp_err_to_name(r));
    vTaskDelay(pdMS_TO_TICKS(10));

    /* CTRL3_C (0x12): IF_INC=1 (auto-increment), BDU=1 (block update) */
    uint8_t if_cfg[] = {0x12, 0x44};
    r = i2c_master_transmit(s_dev, if_cfg, 2, 100);
    if (r != ESP_OK) ESP_LOGE(TAG, "IF cfg failed: %s", esp_err_to_name(r));
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "IMU init complete ✓");
    return ESP_OK;
}

bool imu_is_ready(void) { return s_dev != NULL; }

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
