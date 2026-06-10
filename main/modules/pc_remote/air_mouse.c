#include "air_mouse.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modules/imu/imu_driver.h"
#include "modules/pc_remote/ble_hid.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "AIR_MOUSE";
static float sensitivity = 1.0f;
static float gyro_bias_x = 0, gyro_bias_y = 0, gyro_bias_z = 0;
static float prev_gx = 0, prev_gy = 0;
static bool s_enabled = false;

void air_mouse_set_sensitivity(float s) { sensitivity = s; }
void air_mouse_set_enabled(bool en) { s_enabled = en; }
bool air_mouse_is_enabled(void) { return s_enabled; }

esp_err_t air_mouse_init(void) {
    ESP_LOGI(TAG, "Calibrating gyro bias — keep device still...");

    imu_data_t imu;
    int samples = 100;
    float sx = 0, sy = 0, sz = 0;
    int count = 0;

    for (int i = 0; i < samples; i++) {
        if (imu_read(&imu) == ESP_OK) {
            sx += imu.gx; sy += imu.gy; sz += imu.gz;
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (count > 0) {
        gyro_bias_x = sx / count;
        gyro_bias_y = sy / count;
        gyro_bias_z = sz / count;
    }

    ESP_LOGI(TAG, "Gyro bias: x=%.3f y=%.3f z=%.3f (%d samples)",
             gyro_bias_x, gyro_bias_y, gyro_bias_z, count);
    return ESP_OK;
}

void air_mouse_task(void *arg) {
    imu_data_t imu;
    while (1) {
        if (s_enabled && imu_read(&imu) == ESP_OK) {
            /* Subtract bias, then low-pass filter */
            float raw_x = imu.gx - gyro_bias_x;
            float raw_y = imu.gy - gyro_bias_y;
            float gx = raw_x * 0.7f + prev_gx * 0.3f;
            float gy = raw_y * 0.7f + prev_gy * 0.3f;
            prev_gx = gx; prev_gy = gy;

            /* Dead zone: ignore sensor noise drift */
            if (fabsf(gx) > 1.5f || fabsf(gy) > 1.5f) {
                int8_t dx = (int8_t)fminf(fmaxf(-gx * sensitivity * 2, -127), 127);
                int8_t dy = (int8_t)fminf(fmaxf(gy * sensitivity * 2, -127), 127);
                ble_hid_send_mouse(0, dx, dy);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
