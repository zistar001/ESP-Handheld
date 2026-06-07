#pragma once
#include "esp_err.h"

esp_err_t weather_init(void);
void weather_set_api_key(const char *key);
void weather_set_location(const char *loc);
