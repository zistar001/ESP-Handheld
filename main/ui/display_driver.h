#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t ui_display_init(void);
void ui_display_set_nes_active(bool active);

/* LVGL互斥锁 — LVGL非线程安全，所有外部调用需持锁 */
void lvgl_lock(void);
void lvgl_unlock(void);
