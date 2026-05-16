#pragma once
#include <stdbool.h>
#include "esp_err.h"
esp_err_t sd_card_init(void);
bool sd_card_is_mounted(void);
const char *sd_card_mount_point(void);
