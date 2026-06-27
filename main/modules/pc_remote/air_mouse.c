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

/* ── Debug rolling log (ring buffer, logged only when ENA) ── */
#define DBG_RING_SIZE 16
static float dbg_t[DBG_RING_SIZE];
static float dbg_gx[DBG_RING_SIZE], dbg_gy[DBG_RING_SIZE];
static int8_t dbg_dx[DBG_RING_SIZE], dbg_dy[DBG_RING_SIZE];
static int dbg_wp = 0;

void air_mouse_set_sensitivity(float s) { sensitivity = s; }
void air_mouse_set_enabled(bool en) { s_enabled = en; }
bool air_mouse_is_enabled(void) { return s_enabled; }

esp_err_t air_mouse_init(void) {
    ESP_LOGI(TAG, "Calibrating gyro bias — keep device still...");

    imu_data_t imu;
    int samples = 200;
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

    ESP_LOGI(TAG, "Gyro bias (raw dps): x=%.4f y=%.4f z=%.4f (%d samples)",
             gyro_bias_x, gyro_bias_y, gyro_bias_z, count);
    return ESP_OK;
}

void air_mouse_task(void *arg) {
    imu_data_t imu;
    uint32_t cycle = 0;

    while (1) {
        if (s_enabled && imu_read(&imu) == ESP_OK) {
            /* Subtract bias, then low-pass filter */
            float raw_x = imu.gx - gyro_bias_x;
            float raw_y = imu.gy - gyro_bias_y;
            float gx = raw_x * 0.7f + prev_gx * 0.3f;
            float gy = raw_y * 0.7f + prev_gy * 0.3f;
            prev_gx = gx; prev_gy = gy;

            int8_t dx = 0, dy = 0;
            bool active = false;

            /* Dead zone: ignore sensor noise drift */
            if (fabsf(gx) > 1.5f || fabsf(gy) > 1.5f) {
                active = true;
                dx = (int8_t)fminf(fmaxf(-gx * sensitivity * 2, -127), 127);
                dy = (int8_t)fminf(fmaxf(-gy * sensitivity * 2, -127), 127);
                ble_hid_send_mouse(0, dx, dy);
            }

            /* ── Debug ring buffer ── */
            dbg_t[dbg_wp] = imu.temp;
            dbg_gx[dbg_wp] = gx;
            dbg_gy[dbg_wp] = gy;
            dbg_dx[dbg_wp] = dx;
            dbg_dy[dbg_wp] = dy;
            dbg_wp = (dbg_wp + 1) % DBG_RING_SIZE;

            /* ── Periodic debug dump (every ~100 cycles = ~2s) ── */
            if (++cycle % 100 == 0) {
                ESP_LOGI(TAG, "─── Air Mouse Debug ───");
                ESP_LOGI(TAG, "Bias: x=%.4f y=%.4f z=%.4f | Temp=%.1f°C",
                         gyro_bias_x, gyro_bias_y, gyro_bias_z, imu.temp);
                ESP_LOGI(TAG, "Raw (post-bias): gx=%.3f gy=%.3f (imu: %.4f %.4f)",
                         raw_x, raw_y, imu.gx, imu.gy);
                ESP_LOGI(TAG, "Filtered: gx=%.3f gy=%.3f | DeadZone=%s | dx=%d dy=%d",
                         gx, gy, active ? "OFF" : "ON(blocked)", dx, dy);

                /* Show last 16 samples in a compact line */
                ESP_LOGI(TAG, "Last %d filtered gyro samples (gx,gy):", DBG_RING_SIZE);
                for (int i = 0; i < DBG_RING_SIZE; i++) {
                    int idx = (dbg_wp + i) % DBG_RING_SIZE;
                    ESP_LOGI(TAG, "  [%2d] gx=%+.4f gy=%+.4f dx=%3d dy=%3d",
                             i, dbg_gx[idx], dbg_gy[idx], dbg_dx[idx], dbg_dy[idx]);
                }
                ESP_LOGI(TAG, "─── End Debug ───");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
