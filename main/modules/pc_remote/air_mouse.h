#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t air_mouse_init(void);
void air_mouse_set_sensitivity(float s);
void air_mouse_set_enabled(bool en);
bool air_mouse_is_enabled(void);
void air_mouse_task(void *arg);
