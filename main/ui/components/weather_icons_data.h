#pragma once
#include "lvgl.h"

extern const lv_img_dsc_t icon_sunny;
extern const lv_img_dsc_t icon_sunny_sm;
extern const lv_img_dsc_t icon_cloudy;
extern const lv_img_dsc_t icon_cloudy_sm;
extern const lv_img_dsc_t icon_partly_cloudy;
extern const lv_img_dsc_t icon_partly_cloudy_sm;
extern const lv_img_dsc_t icon_rainy;
extern const lv_img_dsc_t icon_rainy_sm;
extern const lv_img_dsc_t icon_storm;
extern const lv_img_dsc_t icon_storm_sm;
extern const lv_img_dsc_t icon_snow;
extern const lv_img_dsc_t icon_snow_sm;
extern const lv_img_dsc_t icon_foggy;
extern const lv_img_dsc_t icon_foggy_sm;
extern const lv_img_dsc_t icon_windy;
extern const lv_img_dsc_t icon_windy_sm;

const lv_img_dsc_t *weather_icon_get_img(const char *desc, int size);
