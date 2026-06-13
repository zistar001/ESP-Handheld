#include "imu_calib.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include <math.h>

static const char *TAG = "IMU_CALIB";
#define NVS_NS "imu_calib"

static imu_calib_t s_cache = {0};
static bool s_loaded = false;

bool imu_calib_save(const imu_calib_t *calib) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_blob(h, "data", calib, sizeof(*calib));
    nvs_set_i8(h, "valid", 1);
    esp_err_t e = nvs_commit(h);
    nvs_close(h);
    if (e == ESP_OK) {
        s_cache = *calib;
        s_loaded = true;
        ESP_LOGI(TAG, "Calibration saved");
        return true;
    }
    return false;
}

bool imu_calib_load(imu_calib_t *calib) {
    if (s_loaded) {
        *calib = s_cache;
        return true;
    }
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    int8_t v = 0;
    nvs_get_i8(h, "valid", &v);
    if (!v) { nvs_close(h); return false; }
    size_t sz = sizeof(*calib);
    esp_err_t e = nvs_get_blob(h, "data", calib, &sz);
    nvs_close(h);
    if (e == ESP_OK) {
        s_cache = *calib;
        s_loaded = true;
        return true;
    }
    return false;
}

void imu_calib_clear(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    memset(&s_cache, 0, sizeof(s_cache));
    s_loaded = false;
    ESP_LOGI(TAG, "Calibration cleared");
}

static float dot(const imu_vec_t *a, const imu_vec_t *b) {
    return a->ax * b->ax + a->ay * b->ay + a->az * b->az;
}

static float mag2(const imu_vec_t *v) {
    return v->ax * v->ax + v->ay * v->ay + v->az * v->az;
}

imu_pose_t imu_calib_detect(const imu_data_t *imu) {
    imu_calib_t c;
    if (!imu_calib_load(&c)) {
        ESP_LOGW(TAG, "No calibration, using legacy 0.7g threshold");
        float ax = imu->ax, ay = imu->ay, az = imu->az;
        if (az > -0.7f && az < 0.7f) {
            if      (ax < -0.7f) return IMU_POSE_LEFT;
            else if (ay >  0.7f) return IMU_POSE_RIGHT;
            else if (ay < -0.7f) return IMU_POSE_UPRIGHT;
        } else if (az < -0.85f) {
            return IMU_POSE_FLAT;
        }
        return IMU_POSE_UNKNOWN;
    }

    imu_vec_t cur = { imu->ax, imu->ay, imu->az };
    float cur_mag = sqrtf(mag2(&cur));
    if (cur_mag < 0.1f) return IMU_POSE_UNKNOWN;

    const imu_vec_t *poses[] = { &c.upright, &c.left, &c.right, &c.flat };
    float best_dot = -2.0f;
    int best = -1;

    for (int i = 0; i < 4; i++) {
        float p_mag = sqrtf(mag2(poses[i]));
        if (p_mag < 0.1f) continue;
        float d = dot(&cur, poses[i]) / (cur_mag * p_mag);
        if (d > best_dot) { best_dot = d; best = i; }
    }

    if (best < 0 || best_dot < 0.5f) return IMU_POSE_UNKNOWN;
    return (imu_pose_t)(best + 1);
}

const char* imu_pose_name(imu_pose_t p) {
    switch (p) {
        case IMU_POSE_UPRIGHT: return "UPRIGHT";
        case IMU_POSE_LEFT:    return "LEFT";
        case IMU_POSE_RIGHT:   return "RIGHT";
        case IMU_POSE_FLAT:    return "FLAT";
        default:               return "UNKNOWN";
    }
}
