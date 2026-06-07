#include "settings_screen.h"
#include "bsp_board.h"
#include "modules/settings/settings_manager.h"
#include "es8311_driver.h"
#include <stdio.h>

static lv_obj_t *vol_slider, *bright_slider, *mic_slider, *sens_slider, *sleep_slider;
static lv_obj_t *vol_lbl_val, *bright_lbl_val, *mic_lbl_val, *sens_lbl_val, *sleep_lbl_val;
static lv_obj_t *wifi_info;
static lv_obj_t *sleep_switch;
static settings_t current;

static void save_and_update(void) {
    settings_save(&current);
    extern void settings_sync_global(void);
    settings_sync_global();
    if (vol_lbl_val) { char b[8]; snprintf(b,8,"%d%%",current.volume); lv_label_set_text(vol_lbl_val,b); }
    if (bright_lbl_val) { char b[8]; snprintf(b,8,"%d%%",current.brightness); lv_label_set_text(bright_lbl_val,b); }
    if (mic_lbl_val) { char b[8]; snprintf(b,8,"%d%%",current.mic_gain); lv_label_set_text(mic_lbl_val,b); }
    if (sens_lbl_val) { char b[16]; snprintf(b,16,"%.1f",current.air_mouse_sens); lv_label_set_text(sens_lbl_val,b); }
    if (sleep_lbl_val) { char b[16]; snprintf(b,16,"%ds",current.sleep_timeout_sec); lv_label_set_text(sleep_lbl_val,b); }
}

static void vol_slider_cb(lv_event_t *e) {
    current.volume = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    es8311_set_volume(current.volume);
    save_and_update();
}
static void bright_slider_cb(lv_event_t *e) {
    current.brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    bsp_lcd_backlight_set(current.brightness);
    save_and_update();
}
static void mic_slider_cb(lv_event_t *e) {
    current.mic_gain = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
    save_and_update();
}
static void sens_slider_cb(lv_event_t *e) {
    current.air_mouse_sens = lv_slider_get_value(lv_event_get_target(e)) / 10.0f;
    save_and_update();
}
static void sleep_slider_cb(lv_event_t *e) {
    current.sleep_timeout_sec = (uint16_t)lv_slider_get_value(lv_event_get_target(e));
    save_and_update();
}
static void sleep_switch_cb(lv_event_t *e) {
    current.sleep_enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    settings_save(&current);
}

/* Helper: create a labeled slider row */
static int add_slider_row(lv_obj_t *scr, int y, const char *name,
                          lv_obj_t **slider, lv_obj_t **val_lbl,
                          int min, int max, int val,
                          lv_event_cb_t cb) {
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_pos(lbl, 10, y);
    lv_obj_set_size(lbl, 100, 14);

    *slider = lv_slider_create(scr);
    lv_obj_set_size(*slider, 100, 10);
    lv_obj_set_pos(*slider, 110, y);
    lv_slider_set_range(*slider, min, max);
    lv_slider_set_value(*slider, val, LV_ANIM_OFF);
    lv_obj_add_event_cb(*slider, cb, LV_EVENT_VALUE_CHANGED, NULL);

    *val_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(*val_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(*val_lbl, 215, y);

    return y + 22;
}

lv_obj_t *settings_screen_create(const settings_t *s) {
    current = *s;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    int y = 5;

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings (B: Back)");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(title, 10, y);
    y += 25;

    /* Volume */
    y = add_slider_row(scr, y, "Volume", &vol_slider, &vol_lbl_val,
                       0, 100, current.volume, vol_slider_cb);

    /* Brightness */
    y = add_slider_row(scr, y, "Brightness", &bright_slider, &bright_lbl_val,
                       0, 100, current.brightness, bright_slider_cb);

    /* Mic Gain */
    y = add_slider_row(scr, y, "Mic Gain", &mic_slider, &mic_lbl_val,
                       0, 100, current.mic_gain, mic_slider_cb);

    /* Air Mouse Sensitivity (0.1-5.0 -> slider 1-50) */
    y = add_slider_row(scr, y, "Mouse Sens", &sens_slider, &sens_lbl_val,
                       1, 50, (int)(current.air_mouse_sens * 10), sens_slider_cb);

    /* Sleep Timeout (30-600s) */
    y = add_slider_row(scr, y, "Sleep TO", &sleep_slider, &sleep_lbl_val,
                       30, 600, current.sleep_timeout_sec, sleep_slider_cb);

    /* Sleep Enable toggle */
    {
        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, "Sleep Enable");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_pos(lbl, 10, y);

        sleep_switch = lv_switch_create(scr);
        lv_obj_set_pos(sleep_switch, 160, y - 3);
        if (current.sleep_enabled) lv_obj_add_state(sleep_switch, LV_STATE_CHECKED);
        lv_obj_add_event_cb(sleep_switch, sleep_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
        y += 30;
    }

    /* WiFi info */
    wifi_info = lv_label_create(scr);
    lv_obj_set_style_text_color(wifi_info, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(wifi_info, 10, y);
    lv_label_set_text_fmt(wifi_info, "WiFi: %s",
                          current.wifi_ssid[0] ? current.wifi_ssid : "not set");
    y += 16;

    /* Apply current values */
    save_and_update();

    lv_scr_load(scr);
    return scr;
}

void settings_screen_update(const settings_t *s) {
    current = *s;
    if (vol_slider) lv_slider_set_value(vol_slider, s->volume, LV_ANIM_OFF);
    if (bright_slider) lv_slider_set_value(bright_slider, s->brightness, LV_ANIM_OFF);
    if (mic_slider) lv_slider_set_value(mic_slider, s->mic_gain, LV_ANIM_OFF);
    if (sens_slider) lv_slider_set_value(sens_slider, (int)(s->air_mouse_sens * 10), LV_ANIM_OFF);
    if (sleep_slider) lv_slider_set_value(sleep_slider, s->sleep_timeout_sec, LV_ANIM_OFF);
    save_and_update();
    if (wifi_info) {
        lv_label_set_text_fmt(wifi_info, "WiFi: %s",
                              s->wifi_ssid[0] ? s->wifi_ssid : "not set");
    }
}

uint8_t settings_screen_get_volume(void) { return current.volume; }
uint8_t settings_screen_get_brightness(void) { return current.brightness; }
