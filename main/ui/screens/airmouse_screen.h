#pragma once
#include "lvgl.h"
#include <stdbool.h>

lv_obj_t *airmouse_screen_create(void);
bool airmouse_screen_mouse_enabled(void);
void airmouse_screen_navigate(int dx, int dy);
void airmouse_screen_select(void);
