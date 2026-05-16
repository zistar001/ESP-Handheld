#include "imu_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#define ADDR 0x6A
#define I2C I2C_NUM_0
static const char *TAG="IMU";

esp_err_t imu_init(void) {
    uint8_t who;
    esp_err_t r=i2c_master_write_read_device(I2C,ADDR,(uint8_t[]){0x0F},1,&who,1,pdMS_TO_TICKS(100));
    if(r!=ESP_OK||who!=0x6C){ ESP_LOGE(TAG,"IMU not found 0x%02X",who); return ESP_ERR_NOT_FOUND; }
    ESP_LOGI(TAG,"IMU detected");
    i2c_master_write_to_device(I2C,ADDR,(uint8_t[]){0x10,0x40},2,pdMS_TO_TICKS(100)); // ACC 104Hz
    i2c_master_write_to_device(I2C,ADDR,(uint8_t[]){0x11,0x40},2,pdMS_TO_TICKS(100)); // GYRO 104Hz
    i2c_master_write_to_device(I2C,ADDR,(uint8_t[]){0x12,0x44},2,pdMS_TO_TICKS(100)); // IF_INC
    return ESP_OK;
}

static int16_t read_i16(uint8_t reg){
    uint8_t b[2];
    i2c_master_write_read_device(I2C,ADDR,&reg,1,b,2,pdMS_TO_TICKS(100));
    return (int16_t)(b[0]|(b[1]<<8));
}

esp_err_t imu_read(imu_data_t *d){
    d->temp=(float)read_i16(0x20)/256.0f+25.0f;
    d->gx=(float)read_i16(0x22)/70.0f; d->gy=(float)read_i16(0x24)/70.0f; d->gz=(float)read_i16(0x26)/70.0f;
    d->ax=(float)read_i16(0x28)/16384.0f; d->ay=(float)read_i16(0x2A)/16384.0f; d->az=(float)read_i16(0x2C)/16384.0f;
    return ESP_OK;
}
