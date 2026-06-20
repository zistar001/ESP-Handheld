#pragma once
#include "lvgl.h"

lv_obj_t *spectrum_screen_create(void);
void      spectrum_screen_navigate(int dir);
void      spectrum_screen_adjust(int dir);   /* LEFT=-1, RIGHT=+1, A=+1 */
