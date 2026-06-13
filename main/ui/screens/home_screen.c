#include "home_screen.h"
#include "ui/components/weather_icon.h"
#include <stdio.h>
#include <string.h>

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_ORANGE   lv_color_hex(0xFF5C00)
#define COLOR_WHITE    lv_color_hex(0xFFFFFF)
#define COLOR_GREY     lv_color_hex(0x999999)
#define COLOR_WHITE_80 lv_color_hex(0xFFFFFFCC)
#define COLOR_WHITE_67 lv_color_hex(0xFFFFFFAA)
#define COLOR_WHITE_53 lv_color_hex(0xFFFFFF88)
#define COLOR_SEP      lv_color_hex(0xFFFFFF20)

static lv_obj_t *s_scr = NULL;
static lv_obj_t *wifi_icon, *bat_icon, *bat_pct;
static lv_obj_t *time_lbl, *date_lbl;
static lv_obj_t *temp_lbl, *temp_unit_lbl, *city_lbl, *range_lbl;
static lv_obj_t *desc_lbl;
static lv_obj_t *fore_icon[3], *fore_temp[3], *fore_day[3];
static lv_obj_t *indoor_label, *indoor_temp, *indoor_hum;

static char s_cached_time[16], s_cached_date[16];
static char s_cached_city[32], s_cached_desc[32];
static int  s_cached_temp, s_cached_high, s_cached_low;
static float s_cached_indoor_temp, s_cached_indoor_hum;
static bool s_cached_data_valid = false;

lv_obj_t *home_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COLOR_BG, 0);

    wifi_icon = bat_icon = bat_pct = NULL;
    time_lbl = date_lbl = NULL;
    temp_lbl = temp_unit_lbl = city_lbl = range_lbl = desc_lbl = NULL;
    indoor_label = indoor_temp = indoor_hum = NULL;
    for (int i = 0; i < 3; i++) { fore_icon[i] = fore_temp[i] = fore_day[i] = NULL; }

    /* Status Bar */
    wifi_icon = lv_label_create(s_scr);
    lv_label_set_text(wifi_icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(wifi_icon, COLOR_WHITE_80, 0);
    lv_obj_set_pos(wifi_icon, 44, 7);

    lv_obj_t *bt = lv_label_create(s_scr);
    lv_label_set_text(bt, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(bt, COLOR_WHITE_80, 0);
    lv_obj_set_pos(bt, 98, 6);

    bat_icon = lv_label_create(s_scr);
    lv_label_set_text(bat_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(bat_icon, COLOR_WHITE_80, 0);
    lv_obj_set_pos(bat_icon, 150, 5);

    bat_pct = lv_label_create(s_scr);
    lv_label_set_text(bat_pct, "---");
    lv_obj_set_style_text_color(bat_pct, COLOR_WHITE_80, 0);
    lv_obj_set_style_text_font(bat_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(bat_pct, 178, 7);

    /* Time (15,34) 48px */
    time_lbl = lv_label_create(s_scr);
    lv_label_set_text(time_lbl, "--:--");
    lv_obj_set_style_text_color(time_lbl, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(time_lbl, 15, 34);

    /* Date (39,93) 14px */
    date_lbl = lv_label_create(s_scr);
    lv_label_set_text(date_lbl, "----/--/--");
    lv_obj_set_style_text_color(date_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(date_lbl, 39, 93);

    /* Temp (18,106) 48px orange */
    temp_lbl = lv_label_create(s_scr);
    lv_label_set_text(temp_lbl, "--");
    lv_obj_set_style_text_color(temp_lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(temp_lbl, 18, 106);

    /* Unit (77,118) 16px */
    temp_unit_lbl = lv_label_create(s_scr);
    lv_label_set_text(temp_unit_lbl, "\xC2\xB0""C");
    lv_obj_set_style_text_color(temp_unit_lbl, COLOR_GREY, 0);
    lv_obj_set_pos(temp_unit_lbl, 77, 118);

    /* City (164,116) 16px */
    city_lbl = lv_label_create(s_scr);
    lv_label_set_text(city_lbl, "---");
    lv_obj_set_style_text_color(city_lbl, COLOR_WHITE_80, 0);
    lv_obj_set_style_text_font(city_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(city_lbl, 164, 116);

    /* Weather Desc (80,138) 16px */
    desc_lbl = lv_label_create(s_scr);
    lv_label_set_text(desc_lbl, "---");
    lv_obj_set_style_text_color(desc_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(desc_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(desc_lbl, 80, 138);

    /* Temp Range (127,141) 14px */
    range_lbl = lv_label_create(s_scr);
    lv_label_set_text(range_lbl, "--~--\xC2\xB0""C");
    lv_obj_set_style_text_color(range_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(range_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(range_lbl, 127, 141);

    /* 3-Day Forecast - exact Pencil positions */
    /* Day1: icon(34,171) temp(33,201) day(33,217) */
    /* Day2: icon(100,171) temp(103,201) day(105,217) */
    /* Day3: icon(168,171) temp(173,201) day(176,217) */
    int ix[3] = {34, 100, 168};
    int tx[3] = {33, 103, 173};
    int dx[3] = {33, 105, 176};

    for (int i = 0; i < 3; i++) {
        fore_icon[i] = lv_label_create(s_scr);
        lv_label_set_text(fore_icon[i], "\xe6\x99\x93");
        lv_obj_set_style_text_color(fore_icon[i], COLOR_ORANGE, 0);
        lv_obj_set_style_text_font(fore_icon[i], &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(fore_icon[i], ix[i], 171);

        fore_temp[i] = lv_label_create(s_scr);
        lv_label_set_text(fore_temp[i], "--~--\xC2\xB0""C");
        lv_obj_set_style_text_color(fore_temp[i], COLOR_WHITE_67, 0);
        lv_obj_set_style_text_font(fore_temp[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(fore_temp[i], tx[i], 201);

        fore_day[i] = lv_label_create(s_scr);
        lv_label_set_text(fore_day[i], "--/--");
        lv_obj_set_style_text_color(fore_day[i], COLOR_WHITE_53, 0);
        lv_obj_set_style_text_font(fore_day[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(fore_day[i], dx[i], 217);
    }

    /* Separator (131,248) 4x20 */
    lv_obj_t *sep = lv_obj_create(s_scr);
    lv_obj_set_size(sep, 4, 20);
    lv_obj_set_pos(sep, 131, 248);
    lv_obj_set_style_bg_color(sep, COLOR_SEP, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    /* Indoor (34,245) (76,248) (150,248) */
    indoor_label = lv_label_create(s_scr);
    lv_label_set_text(indoor_label, "\xe5\xae\xa4\xe5\x86\x85");
    lv_obj_set_style_text_color(indoor_label, COLOR_GREY, 0);
    lv_obj_set_style_text_font(indoor_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(indoor_label, 34, 245);

    indoor_temp = lv_label_create(s_scr);
    lv_label_set_text(indoor_temp, "--\xC2\xB0""C");
    lv_obj_set_style_text_color(indoor_temp, COLOR_ORANGE, 0);
    lv_obj_set_pos(indoor_temp, 76, 248);

    indoor_hum = lv_label_create(s_scr);
    lv_label_set_text(indoor_hum, "--%");
    lv_obj_set_style_text_color(indoor_hum, COLOR_ORANGE, 0);
    lv_obj_set_pos(indoor_hum, 150, 248);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);

    if (s_cached_data_valid) {
        if (time_lbl) lv_label_set_text(time_lbl, s_cached_time);
        if (date_lbl) lv_label_set_text(date_lbl, s_cached_date);
        if (city_lbl && *s_cached_city) lv_label_set_text(city_lbl, s_cached_city);
        if (desc_lbl && *s_cached_desc) lv_label_set_text(desc_lbl, s_cached_desc);
        if (temp_lbl && s_cached_temp) { char buf[16]; snprintf(buf, 16, "%d", s_cached_temp); lv_label_set_text(temp_lbl, buf); }
        if (range_lbl && (s_cached_high || s_cached_low)) { char buf[32]; snprintf(buf, 32, "%d~%d\xC2\xB0""C", s_cached_low, s_cached_high); lv_label_set_text(range_lbl, buf); }
        if (indoor_temp && s_cached_indoor_temp) { char buf[16]; snprintf(buf, 16, "%.1f\xC2\xB0""C", s_cached_indoor_temp); lv_label_set_text(indoor_temp, buf); }
        if (indoor_hum && s_cached_indoor_hum) { char buf[16]; snprintf(buf, 16, "%.0f%%", s_cached_indoor_hum); lv_label_set_text(indoor_hum, buf); }
    }
    return s_scr;
}

void home_screen_update_time(const char *time_str, const char *date_str) {
    if (time_str) { strncpy(s_cached_time, time_str, 15); s_cached_data_valid = true; }
    if (date_str) strncpy(s_cached_date, date_str, 15);
    if (time_lbl) lv_label_set_text(time_lbl, time_str ? time_str : "--:--");
    if (date_lbl) lv_label_set_text(date_lbl, date_str ? date_str : "");
}

void home_screen_update_weather(const char *city, const char *desc, int temp, int temp_high, int temp_low, const char *icon) {
    char buf[64];
    s_cached_temp = temp; s_cached_high = temp_high; s_cached_low = temp_low;
    if (city) strncpy(s_cached_city, city, 31);
    if (desc) strncpy(s_cached_desc, desc, 31);
    s_cached_data_valid = true;
    if (city_lbl && city) lv_label_set_text(city_lbl, city);
    if (desc_lbl && desc) lv_label_set_text(desc_lbl, desc);
    if (temp_lbl && temp > -50) { snprintf(buf, 64, "%d", temp); lv_label_set_text(temp_lbl, buf); }
    if (range_lbl && (temp_high || temp_low)) { snprintf(buf, 64, "%d~%d\xC2\xB0""C", temp_low, temp_high); lv_label_set_text(range_lbl, buf); }
}

void home_screen_update_wifi(bool connected) {
    if (wifi_icon) lv_label_set_text(wifi_icon, connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
}

void home_screen_update_forecast(int day_index, const char *date, int temp_high, int temp_low) {
    if (day_index < 0 || day_index >= 3) return;
    char buf[32];
    if (fore_temp[day_index]) { snprintf(buf, 32, "%d~%d\xC2\xB0""C", temp_low, temp_high); lv_label_set_text(fore_temp[day_index], buf); }
    if (fore_day[day_index] && date && strlen(date) >= 10) { snprintf(buf, 32, "%c%c/%c%c", date[5], date[6], date[8], date[9]); lv_label_set_text(fore_day[day_index], buf); }
}

void home_screen_update_indoor(float temp, float humi) {
    char buf[32];
    s_cached_indoor_temp = temp; s_cached_indoor_hum = humi;
    if (indoor_temp) { snprintf(buf, 32, "%.1f\xC2\xB0""C", temp); lv_label_set_text(indoor_temp, buf); }
    if (indoor_hum) { snprintf(buf, 32, "%.0f%%", humi); lv_label_set_text(indoor_hum, buf); }
}

void home_screen_update_battery(int pct, bool charging) {
    char buf[16];
    if (pct < 0) { if (bat_pct) lv_label_set_text(bat_pct, "---"); }
    else {
        if (bat_pct) { snprintf(buf, 16, "%d%%", pct); lv_label_set_text(bat_pct, buf); }
        if (bat_icon) {
            const char *sym = charging ? LV_SYMBOL_CHARGE :
                (pct > 60 ? LV_SYMBOL_BATTERY_FULL : pct > 40 ? LV_SYMBOL_BATTERY_3 : pct > 20 ? LV_SYMBOL_BATTERY_2 : pct > 5 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY);
            lv_label_set_text(bat_icon, sym);
            lv_obj_set_style_text_color(bat_icon, charging ? lv_color_hex(0x4CAF50) : COLOR_WHITE_80, 0);
        }
    }
}
