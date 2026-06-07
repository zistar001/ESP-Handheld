#pragma once
#include "lvgl.h"
#include "modules/settings/settings_manager.h"

lv_obj_t *settings_screen_create(const settings_t *s);
void settings_screen_update(const settings_t *s);
uint8_t settings_screen_get_volume(void);
uint8_t settings_screen_get_brightness(void);
