#pragma once
#include "lvgl.h"
#include <stdbool.h>

lv_obj_t *kbd_screen_create(void);
bool kbd_screen_is_enabled(void);
void kbd_screen_navigate(int dy);
void kbd_screen_select(void);
void kbd_screen_set_voice_active(bool active);
