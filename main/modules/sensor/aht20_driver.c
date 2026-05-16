#include "aht20_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#define ADDR 0x38
#define I2C I2C_NUM_0
static const char *TAG="AHT20";

esp_err_t aht20_init(void) {
    uint8_t c[]={0xBE,0x08,0x00};
    esp_err_t r=i2c_master_write_to_device(I2C,ADDR,c,3,pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(10));
    return r;
}

esp_err_t aht20_read(float *temp, float *humi) {
    uint8_t t[]={0xAC,0x33,0x00};
    esp_err_t r=i2c_master_write_to_device(I2C,ADDR,t,3,pdMS_TO_TICKS(100));
    if(r) return r;
    vTaskDelay(pdMS_TO_TICKS(80));
    uint8_t b[6]={0};
    r=i2c_master_read_from_device(I2C,ADDR,b,6,pdMS_TO_TICKS(100));
    if(r||(b[0]&0x80)) return r?r:ESP_ERR_INVALID_STATE;
    uint32_t rh=((uint32_t)b[1]<<12)|((uint32_t)b[2]<<4)|(b[3]>>4);
    uint32_t rt=((uint32_t)(b[3]&0x0F)<<16)|((uint32_t)b[4]<<8)|b[5];
    *humi = (float)rh/0x100000*100.0f;
    *temp = (float)rt/0x100000*200.0f-50.0f;
    return ESP_OK;
}
