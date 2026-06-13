#include "record_screen.h"
#include "modules/pc_remote/wifi_audio.h"
#include "ui/display_driver.h"
#include "esp_log.h"
static const char *TAG = "RECORD";

static lv_obj_t *status_lbl = NULL;

static void scr_del_cb(lv_event_t *e) {
    (void)e;
    if (wifi_audio_is_streaming()) wifi_audio_voice_stop();
    status_lbl = NULL;
}

lv_obj_t *record_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A0A), 0);
    lv_obj_add_event_cb(scr, scr_del_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "\xe5\xbd\x95\xe9\x9f\xb3");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_center(title);

    status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, wifi_audio_is_streaming() ? "\xe6\xad\xa3\xe5\x9c\xa8\xe5\xbd\x95\xe5\x88\xb6..." : "\xe6\x8c\x89 A \xe5\xbc\x80\xe5\xa7\x8b\xe5\xbd\x95\xe9\x9f\xb3");
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xFF5C00), 0);
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(status_lbl, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void record_screen_toggle(void) {
    if (wifi_audio_is_streaming()) {
        wifi_audio_voice_stop();
        if (status_lbl) lv_label_set_text(status_lbl, "\xe6\x8c\x89 A \xe5\xbc\x80\xe5\xa7\x8b\xe5\xbd\x95\xe9\x9f\xb3");
    } else {
        if (wifi_audio_voice_start() == ESP_OK) {
            if (status_lbl) lv_label_set_text(status_lbl, "\xe6\xad\xa3\xe5\x9c\xa8\xe5\xbd\x95\xe5\x88\xb6...");
        }
    }
}
