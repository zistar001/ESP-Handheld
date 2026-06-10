#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef struct {
    float ax, ay, az;  // accel (g)
    float gx, gy, gz;  // gyro (dps)
    float temp;
} imu_data_t;
esp_err_t imu_init(void);
esp_err_t imu_read(imu_data_t *d);
bool imu_is_ready(void);
