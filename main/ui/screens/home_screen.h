#pragma once
#include "lvgl.h"

lv_obj_t *home_screen_create(void);
void home_screen_update_time(const char *time_str, const char *date_str);
void home_screen_update_weather(const char *city, const char *desc, int temp, int temp_high, int temp_low, const char *icon);
void home_screen_update_wifi(bool connected);
void home_screen_update_battery(int pct, bool charging);
void home_screen_update_indoor(float temp, float humi);
void home_screen_update_forecast(int day_index, const char *date, int temp_high, int temp_low);
