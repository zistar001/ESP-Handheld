#pragma once
#include "esp_err.h"
esp_err_t aht20_init(void);
esp_err_t aht20_read(float *temp, float *humi);
