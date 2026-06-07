#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t battery_monitor_init(void);
int battery_monitor_get_millivolts(void);
int battery_monitor_get_percentage(void);
bool battery_monitor_is_charging(void);
bool battery_monitor_is_charged(void);
