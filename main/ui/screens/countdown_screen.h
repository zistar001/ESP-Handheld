#pragma once
#include "lvgl.h"

lv_obj_t *countdown_screen_create(void);
void countdown_screen_set_time(int minutes, int seconds);
void countdown_screen_set_state(bool running, int preset_idx);
void countdown_screen_update(void);
void countdown_screen_reset(void);
