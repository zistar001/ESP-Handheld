#pragma once
#include "lvgl.h"
#include <stdbool.h>

lv_obj_t *airmouse_screen_create(void);
void airmouse_screen_set_connection(bool connected);
void airmouse_screen_set_sensitivity(int level);  /* 0=low, 1=mid, 2=high */
void airmouse_screen_set_preset(int preset);      /* 0=default, 1=game, 2=office */
