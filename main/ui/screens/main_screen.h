#pragma once
#include "lvgl.h"

lv_obj_t *main_screen_create(void);
void main_screen_update_time(const char *t, const char *d);
void main_screen_update_weather(const char *w, int temp);
void main_screen_update_device_temp(float t, float h);
void main_screen_update_imu(float pitch, float roll);
