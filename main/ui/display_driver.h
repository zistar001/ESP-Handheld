#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t ui_display_init(void);
void ui_display_set_nes_active(bool active);
