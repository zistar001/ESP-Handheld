#include "main_screen.h"
#include "ui/components/status_bar.h"
#include "modules/pc_remote/ble_hid.h"
#include "modules/settings/settings_manager.h"
#include <stdio.h>

static lv_obj_t *time_lbl, *date_lbl, *weather_lbl, *dev_lbl, *imu_lbl, *hint_lbl;
static lv_obj_t *status_bar = NULL;

lv_obj_t *main_screen_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    /* Status bar at top */
    status_bar = status_bar_create(scr);

    /* Shift all content down by 24px to leave room for status bar */
    time_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(time_lbl, "00:00");
    lv_obj_center(time_lbl);
    lv_obj_set_y(time_lbl, -40 + 12);

    date_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(date_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(date_lbl, "2026-01-01");
    lv_obj_align(date_lbl, LV_ALIGN_CENTER, 0, 10 + 12);

    weather_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(weather_lbl, lv_color_hex(0x87CEEB), 0);
    lv_obj_set_style_text_font(weather_lbl, &lv_font_simsun_16_cjk, 0);
    lv_label_set_text(weather_lbl, "---");
    lv_obj_align(weather_lbl, LV_ALIGN_CENTER, 0, 50 + 12);

    dev_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(dev_lbl, lv_color_hex(0x90EE90), 0);
    lv_obj_set_style_text_font(dev_lbl, &lv_font_simsun_16_cjk, 0);
    lv_label_set_text(dev_lbl, "设备: --\xC2\xB0""C  --%");
    lv_obj_align(dev_lbl, LV_ALIGN_CENTER, 0, 80 + 12);

    imu_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(imu_lbl, lv_color_hex(0xFFA500), 0);
    lv_obj_set_style_text_font(imu_lbl, &lv_font_simsun_16_cjk, 0);
    lv_label_set_text(imu_lbl, "俯仰: 0  横滚: 0");
    lv_obj_align(imu_lbl, LV_ALIGN_CENTER, 0, 105 + 12);

    hint_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(hint_lbl, &lv_font_simsun_16_cjk, 0);
    lv_label_set_text(hint_lbl, "START > 菜单");
    lv_obj_align(hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* Init status bar indicators */
    settings_t s;
    settings_load(&s);
    status_bar_set_volume(s.volume);
    status_bar_set_brightness(s.brightness);
    status_bar_set_wifi(strlen(s.wifi_ssid) > 0);
    status_bar_set_ble(ble_hid_is_connected());

    lv_scr_load(scr);
    return scr;
}

void main_screen_update_time(const char *t, const char *d) {
    if (time_lbl) lv_label_set_text(time_lbl, t);
    if (date_lbl) lv_label_set_text(date_lbl, d);
}
void main_screen_update_weather(const char *w, int temp) {
    if (weather_lbl) { char b[64]; snprintf(b,64,"%s  %d\xC2\xB0""C",w,temp); lv_label_set_text(weather_lbl,b); }
}
void main_screen_update_device_temp(float t, float h) {
    if (dev_lbl) { char b[64]; snprintf(b,64,"设备: %.1f\xC2\xB0""C  %.0f%%",t,h); lv_label_set_text(dev_lbl,b); }
}
void main_screen_update_imu(float pitch, float roll) {
    if (imu_lbl) { char b[64]; snprintf(b,64,"俯仰: %+03.0f  横滚: %+03.0f",pitch,roll); lv_label_set_text(imu_lbl,b); }
}
