#pragma once
#include "esp_err.h"

esp_err_t air_mouse_init(void);
void air_mouse_set_sensitivity(float s);
void air_mouse_task(void *arg);
