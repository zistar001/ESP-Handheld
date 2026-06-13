#pragma once
#include "imu_driver.h"
#include <stdbool.h>

typedef enum {
    IMU_POSE_UNKNOWN = 0,
    IMU_POSE_UPRIGHT,
    IMU_POSE_LEFT,
    IMU_POSE_RIGHT,
    IMU_POSE_FLAT,
} imu_pose_t;

typedef struct {
    float ax, ay, az;
} imu_vec_t;

typedef struct {
    imu_vec_t upright;
    imu_vec_t left;
    imu_vec_t right;
    imu_vec_t flat;
    bool valid;
} imu_calib_t;

bool imu_calib_save(const imu_calib_t *calib);
bool imu_calib_load(imu_calib_t *calib);
void imu_calib_clear(void);

imu_pose_t imu_calib_detect(const imu_data_t *imu);
const char* imu_pose_name(imu_pose_t p);
