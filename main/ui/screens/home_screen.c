#include "home_screen.h"
#include <stdio.h>
#include <string.h>

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_ORANGE   lv_color_hex(0xFF5C00)
#define COLOR_WHITE    lv_color_hex(0xFFFFFF)
#define COLOR_GREY     lv_color_hex(0x999999)

/* Static UI elements */
static lv_obj_t *wifi_icon, *bat_icon, *bat_pct;
static lv_obj_t *time_lbl, *date_lbl;
static lv_obj_t *temp_lbl, *temp_unit_lbl, *city_lbl, *range_lbl;
static lv_obj_t *desc_lbl;
static lv_obj_t *fore_icon[3], *fore_temp[3], *fore_day[3];
static lv_obj_t *indoor_label, *indoor_temp, *indoor_hum;
static lv_obj_t *deco_lbl;

lv_obj_t *home_screen_create(void) {
    /* 删除旧屏幕释放LVGL内存 */
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    /* ── Status Bar (28px) ── */
    wifi_icon = lv_label_create(scr);
    lv_label_set_text(wifi_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_pos(wifi_icon, 6, 6);

    bat_pct = lv_label_create(scr);
    lv_label_set_text(bat_pct, "---");
    lv_obj_set_style_text_color(bat_pct, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_pos(bat_pct, 216, 6);

    /* ── Time Area (will be updated by launcher_update_task) ── */
    time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "--:--");
    lv_obj_set_style_text_color(time_lbl, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(time_lbl, 15, 44);

    date_lbl = lv_label_create(scr);
    lv_label_set_text(date_lbl, "----/--/--");
    lv_obj_set_style_text_color(date_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(date_lbl, 15, 92);

    /* ── Background decoration: large sun symbol at low opacity ── */
    deco_lbl = lv_label_create(scr);
    lv_label_set_text(deco_lbl, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(deco_lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(deco_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(deco_lbl, 144, 28);
    lv_obj_set_style_opa(deco_lbl, LV_OPA_10, 0);

    /* ── Weather Area ── */

    /* Left: Temperature */
    temp_lbl = lv_label_create(scr);
    lv_label_set_text(temp_lbl, "--");
    lv_obj_set_style_text_color(temp_lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(temp_lbl, 15, 116);

    temp_unit_lbl = lv_label_create(scr);
    lv_label_set_text(temp_unit_lbl, "\xC2\xB0""C");
    lv_obj_set_style_text_color(temp_unit_lbl, COLOR_GREY, 0);
    lv_obj_set_pos(temp_unit_lbl, 68, 136);

    desc_lbl = lv_label_create(scr);
    lv_label_set_text(desc_lbl, "---");
    lv_obj_set_style_text_color(desc_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(desc_lbl, 15, 164);

    /* Right: City + Range */
    city_lbl = lv_label_create(scr);
    lv_label_set_text(city_lbl, "---");
    lv_obj_set_style_text_color(city_lbl, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_style_text_font(city_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(city_lbl, 118, 120);

    range_lbl = lv_label_create(scr);
    lv_label_set_text(range_lbl, "---");
    lv_obj_set_style_text_color(range_lbl, COLOR_GREY, 0);
    lv_obj_set_pos(range_lbl, 118, 142);

    /* ── 3-Day Forecast ── */
    for (int i = 0; i < 3; i++) {
        fore_icon[i] = lv_label_create(scr);
        lv_label_set_text(fore_icon[i], LV_SYMBOL_OK);
        lv_obj_set_style_text_color(fore_icon[i], COLOR_ORANGE, 0);
        lv_obj_set_pos(fore_icon[i], 15 + i * 70, 188);

        fore_temp[i] = lv_label_create(scr);
        lv_label_set_text(fore_temp[i], "--\xC2\xB0");
        lv_obj_set_style_text_color(fore_temp[i], lv_color_hex(0xFFFFFFAA), 0);
        lv_obj_set_pos(fore_temp[i], 15 + i * 70, 208);

        fore_day[i] = lv_label_create(scr);
        lv_label_set_text(fore_day[i], "---");
        lv_obj_set_style_text_color(fore_day[i], lv_color_hex(0xFFFFFF88), 0);
        lv_obj_set_pos(fore_day[i], 15 + i * 70, 224);
    }

    /* ── Separator line ── */
    lv_obj_t *sLine = lv_obj_create(scr);
    lv_obj_set_size(sLine, 1, 20);
    lv_obj_set_pos(sLine, 119, 250);
    lv_obj_set_style_bg_color(sLine, lv_color_hex(0xFFFFFF20), 0);
    lv_obj_set_style_border_width(sLine, 0, 0);
    lv_obj_set_style_radius(sLine, 0, 0);

    /* ── Indoor Data ── */
    indoor_label = lv_label_create(scr);
    lv_label_set_text(indoor_label, "\xE5\xAE\xA4\xE5\x86\x85"); /* 室内 */
    lv_obj_set_style_text_color(indoor_label, COLOR_GREY, 0);
    lv_obj_set_style_text_font(indoor_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(indoor_label, 60, 254);

    indoor_temp = lv_label_create(scr);
    lv_label_set_text(indoor_temp, "--\xC2\xB0""C");
    lv_obj_set_style_text_color(indoor_temp, COLOR_ORANGE, 0);
    lv_obj_set_pos(indoor_temp, 106, 254);

    indoor_hum = lv_label_create(scr);
    lv_label_set_text(indoor_hum, "--%");
    lv_obj_set_style_text_color(indoor_hum, COLOR_ORANGE, 0);
    lv_obj_set_pos(indoor_hum, 168, 254);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

/* ── Update Functions ── */

void home_screen_update_time(const char *time_str, const char *date_str) {
    if (time_lbl) lv_label_set_text(time_lbl, time_str ? time_str : "--:--");
    if (date_lbl) lv_label_set_text(date_lbl, date_str ? date_str : "");
}

void home_screen_update_weather(const char *city, const char *desc,
                                 int temp, int temp_high, int temp_low,
                                 const char *icon) {
    char buf[64];
    if (city_lbl && city) lv_label_set_text(city_lbl, city);
    if (desc_lbl && desc) lv_label_set_text(desc_lbl, desc);
    if (temp_lbl) {
        snprintf(buf, sizeof(buf), "%d", temp);
        lv_label_set_text(temp_lbl, buf);
    }
    if (range_lbl) {
        snprintf(buf, sizeof(buf),
                 "\xE2\x86\x91%d\xC2\xB0""C  \xE2\x86\x93%d\xC2\xB0""C",
                 temp_high, temp_low);
        lv_label_set_text(range_lbl, buf);
    }
    if (deco_lbl && icon) {
        lv_label_set_text(deco_lbl, LV_SYMBOL_OK);
    }
}

void home_screen_update_wifi(bool connected) {
    if (wifi_icon) {
        lv_label_set_text(wifi_icon, connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    }
}

void home_screen_update_forecast(int day_index, const char *desc, int temp_high, int temp_low) {
    if (day_index < 0 || day_index >= 3) return;
    char buf[32];
    if (fore_day[day_index] && desc) {
        /* Show high/low instead of day name */
        snprintf(buf, sizeof(buf), "%d/%d", temp_high, temp_low);
        lv_label_set_text(fore_day[day_index], buf);
    }
    if (fore_temp[day_index]) {
        snprintf(buf, sizeof(buf), "%d\xC2\xB0", temp_high);
        lv_label_set_text(fore_temp[day_index], buf);
    }
    (void)desc; /* could map to weather icons later */
}

void home_screen_update_indoor(float temp, float humi) {
    char buf[32];
    if (indoor_temp) {
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0""C", temp);
        lv_label_set_text(indoor_temp, buf);
    }
    if (indoor_hum) {
        snprintf(buf, sizeof(buf), "%.0f%%", humi);
        lv_label_set_text(indoor_hum, buf);
    }
}

void home_screen_update_battery(int pct, bool charging) {
    char buf[16];
    if (pct < 0) {
        if (bat_pct) lv_label_set_text(bat_pct, "---");
        if (bat_icon) lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_FULL);
    } else {
        if (bat_pct) {
            snprintf(buf, sizeof(buf), "%d%%", pct);
            lv_label_set_text(bat_pct, buf);
        }
        if (bat_icon) {
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
                lv_obj_set_style_text_color(bat_icon, lv_color_hex(0xFFFFFFCC), 0);
            }
        }
    }
}
