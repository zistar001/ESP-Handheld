#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

lv_obj_t *status_bar_create(lv_obj_t *parent);
void status_bar_set_wifi(bool on);
void status_bar_set_ble(bool on);
void status_bar_set_battery(int pct, bool charging);
void status_bar_update(void);
