#pragma once
#include "lvgl.h"

typedef enum {
    WEATHER_SUNNY = 0,
    WEATHER_CLOUDY,
    WEATHER_PARTLY_CLOUDY,
    WEATHER_RAINY,
    WEATHER_STORM,
    WEATHER_SNOW,
    WEATHER_FOGGY,
    WEATHER_WINDY,
    WEATHER_MAX
} weather_icon_t;

weather_icon_t weather_icon_from_desc(const char *desc);

/* Create a weather icon image object. size=96 for large, 20 for small */
lv_obj_t *weather_icon_create(lv_obj_t *parent, const char *desc, int size);

/* Update an existing weather icon */
void weather_icon_update(lv_obj_t *img, const char *desc, int size);
