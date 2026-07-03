#include "status_bar.h"
#include <stdio.h>

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_WHITE_80 lv_color_hex(0xFFFFFFCC)

static lv_obj_t *s_bar = NULL;
static lv_obj_t *wifi_lbl = NULL, *ble_lbl = NULL, *bat_icon = NULL, *bat_pct = NULL;

/* Cached state - survives screen recreation */
static bool s_wifi_on = false;
static int s_battery_pct = -1;
static bool s_battery_charging = false;

static void status_bar_del_cb(lv_event_t *e) {
    (void)e;
    s_bar = NULL;
    wifi_lbl = NULL;
    ble_lbl = NULL;
    bat_icon = NULL;
    bat_pct = NULL;
}

lv_obj_t *status_bar_create(lv_obj_t *parent) {
    if (s_bar) {
        lv_obj_del(s_bar);
        s_bar = NULL;
    }

    s_bar = lv_obj_create(parent);
    lv_obj_set_size(s_bar, 240, 28);
    lv_obj_set_pos(s_bar, 0, 0);
    lv_obj_set_style_bg_color(s_bar, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_pad_all(s_bar, 0, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_add_event_cb(s_bar, status_bar_del_cb, LV_EVENT_DELETE, NULL);

    /* WiFi at (44, 7) */
    wifi_lbl = lv_label_create(s_bar);
    lv_label_set_text(wifi_lbl, s_wifi_on ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(wifi_lbl, COLOR_WHITE_80, 0);
    lv_obj_set_pos(wifi_lbl, 44, 7);

    /* BT at (98, 6) */
    ble_lbl = lv_label_create(s_bar);
    lv_label_set_text(ble_lbl, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(ble_lbl, COLOR_WHITE_80, 0);
    lv_obj_set_pos(ble_lbl, 98, 6);

    /* Battery icon at (150, 5) */
    bat_icon = lv_label_create(s_bar);
    lv_obj_set_style_text_color(bat_icon, COLOR_WHITE_80, 0);
    lv_obj_set_pos(bat_icon, 150, 5);

    /* Battery percentage at (178, 7) */
    bat_pct = lv_label_create(s_bar);
    lv_obj_set_style_text_color(bat_pct, COLOR_WHITE_80, 0);
    lv_obj_set_style_text_font(bat_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(bat_pct, 178, 7);

    /* Restore cached values */
    if (s_battery_pct < 0) {
        lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_FULL);
        lv_label_set_text(bat_pct, "---");
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", s_battery_pct);
        lv_label_set_text(bat_pct, buf);
        if (s_battery_charging) {
            lv_label_set_text(bat_icon, LV_SYMBOL_CHARGE);
            lv_obj_set_style_text_color(bat_icon, lv_color_hex(0x4CAF50), 0);
        } else {
            const char *sym = LV_SYMBOL_BATTERY_FULL;
            if      (s_battery_pct > 60) sym = LV_SYMBOL_BATTERY_FULL;
            else if (s_battery_pct > 40) sym = LV_SYMBOL_BATTERY_3;
            else if (s_battery_pct > 20) sym = LV_SYMBOL_BATTERY_2;
            else if (s_battery_pct > 5)  sym = LV_SYMBOL_BATTERY_1;
            else               sym = LV_SYMBOL_BATTERY_EMPTY;
            lv_label_set_text(bat_icon, sym);
        }
    }

    return s_bar;
}

void status_bar_set_wifi(bool on) {
    s_wifi_on = on;
    if (wifi_lbl && lv_obj_is_valid(wifi_lbl)) {
        lv_label_set_text(wifi_lbl, on ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    }
}

void status_bar_set_ble(bool on) {
    if (ble_lbl && lv_obj_is_valid(ble_lbl)) {
        lv_label_set_text(ble_lbl, on ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE);
    }
}

void status_bar_set_battery(int pct, bool charging) {
    s_battery_pct = pct;
    s_battery_charging = charging;

    if (!bat_icon || !bat_pct) return;
    if (!lv_obj_is_valid(bat_icon) || !lv_obj_is_valid(bat_pct)) return;

    if (pct < 0) {
        lv_label_set_text(bat_pct, "---");
        lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(bat_icon, COLOR_WHITE_80, 0);
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(bat_pct, buf);
        if (charging) {
            lv_label_set_text(bat_icon, LV_SYMBOL_CHARGE);
            lv_obj_set_style_text_color(bat_icon, lv_color_hex(0x4CAF50), 0);
        } else {
            const char *sym = LV_SYMBOL_BATTERY_FULL;
            if      (pct > 60) sym = LV_SYMBOL_BATTERY_FULL;
            else if (pct > 40) sym = LV_SYMBOL_BATTERY_3;
            else if (pct > 20) sym = LV_SYMBOL_BATTERY_2;
            else if (pct > 5)  sym = LV_SYMBOL_BATTERY_1;
            else               sym = LV_SYMBOL_BATTERY_EMPTY;
            lv_label_set_text(bat_icon, sym);
            lv_obj_set_style_text_color(bat_icon, COLOR_WHITE_80, 0);
        }
    }
}
