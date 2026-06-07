#pragma once
#include "lvgl.h"

lv_obj_t *menu_screen_create(void);
void menu_screen_navigate(int dx, int dy);
void menu_screen_select(void);
