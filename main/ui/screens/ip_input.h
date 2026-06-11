#pragma once
#include "lvgl.h"

lv_obj_t *ip_input_screen_create(void);
void ip_input_navigate(int dx, int dy);   /* dx: switch digit, dy: change value */
void ip_input_select(void);               /* confirm */
void ip_input_cancel(void);               /* cancel */
