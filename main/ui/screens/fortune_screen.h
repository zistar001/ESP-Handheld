#pragma once
#include "lvgl.h"

lv_obj_t *fortune_screen_create(void);
void fortune_screen_navigate(int dir);
void fortune_screen_select(void);
