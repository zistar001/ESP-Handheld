#include "settings_screen.h"
#include "app/app_manager.h"
#include "bsp_board.h"
#include "modules/settings/settings_manager.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "modules/pc_remote/wifi_audio.h"
#include "modules/weather/weather.h"
#include "modules/theme/theme_manager.h"
#include "ui/components/status_bar.h"
#include "es8311_driver.h"
#include "modules/audio/box_audio_codec.h"
#include <stdio.h>
#include <string.h>

/* Styling */
#include "ui/components/theme_colors.h"
#define COLOR_BG     CLR_BG
#define COLOR_ROW    lv_color_hex(0x151515)
#define COLOR_ORANGE CLR_ACCENT
#define COLOR_GREY   lv_color_hex(0x666666)
#define COLOR_LINE   lv_color_hex(0x2A2A2A)

#define ITEM_H 30
#define MAX_ITEMS 9

typedef struct {
    const char *name;
    const char *(*get_val)(void);
    void (*on_enter)(void);
} settings_item_t;

/* === Sub-screen state === */
static int s_sub_mode = 0;
static int s_sub_val = 0;
static bool s_sub_bool = false;
static int s_sel = 0;
static lv_obj_t *s_list_cont = NULL;   /* 主设置列表的滚动容器 */
static lv_obj_t *s_loc_rows[16] = {NULL}; /* 地区列表行指针（仿占卜模块） */

/* ================================================================
 *  Sub-screen helpers
 * ================================================================ */

static void recreate_list(void) {
    s_sub_mode = 0;
    s_sel = 0;
    settings_t s;
    settings_load(&s);
    settings_screen_create(&s);
}

static void show_sub_screen(const char *title, const char *hint) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    status_bar_create(scr);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(t, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(t, 240);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *h = lv_label_create(scr);
    lv_label_set_text(h, hint ? hint : "UP/DOWN:调整  A:保存  B:返回");
    lv_obj_set_style_text_color(h, COLOR_GREY, 0);
    lv_obj_set_style_text_font(h, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(h, 240);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
}

static void update_sub_display(void) {
    if (s_sub_mode == 4) return;  /* 子模式4 有自己的滚动列表，不用单行显示 */
    lv_obj_t *scr = lv_scr_act();
    if (!scr) return;
    lv_obj_t *v = lv_obj_get_child(scr, 2);
    if (v) lv_obj_del(v);

    char buf[48];
    lv_obj_t *val = lv_label_create(scr);
    switch (s_sub_mode) {
        case 1: snprintf(buf, sizeof(buf), "音量: %d%%", s_sub_val); break;
        case 2: snprintf(buf, sizeof(buf), "亮度: %d%%", s_sub_val); break;
        case 3: snprintf(buf, sizeof(buf), "睡眠: %s   %d秒",
                         s_sub_bool ? "开启" : "关闭", s_sub_val); break;
        case 4: {
            const weather_loc_entry_t *locs = weather_get_locations();
            int n = weather_get_location_count();
            if (s_sub_val >= 0 && s_sub_val < n)
                snprintf(buf, sizeof(buf), "%s", locs[s_sub_val].name);
            else
                snprintf(buf, sizeof(buf), "泉州市区");
            break;
        }
        default: return;
    }
    lv_label_set_text(val, buf);
    lv_obj_set_style_text_color(val, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(val, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(val, 240);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, -10);
}

/* ================================================================
 *  Entry callbacks — sub-screens (stay in settings)
 * ================================================================ */
static void enter_volume(void) {
    settings_t s; settings_load(&s);
    s_sub_mode = 1;
    s_sub_val = s.volume;
    show_sub_screen("音量设置", NULL);
    update_sub_display();
}

static void enter_brightness(void) {
    settings_t s; settings_load(&s);
    s_sub_mode = 2;
    s_sub_val = s.brightness;
    show_sub_screen("亮度设置", NULL);
    update_sub_display();
}

static void enter_sleep(void) {
    settings_t s; settings_load(&s);
    s_sub_mode = 3;
    s_sub_val = s.sleep_timeout_sec;
    s_sub_bool = s.sleep_enabled;
    show_sub_screen("睡眠设置", "A:开关  UP/DOWN:超时  B:保存返回");
    update_sub_display();
}

/* ================================================================
 *  Entry callbacks — full apps (return to settings)
 * ================================================================ */
static void enter_wifi(void) {
    app_manager_set_return_to(APP_ID_SETTINGS);
    app_manager_launch(APP_ID_WIFI_SETUP);
}
static void enter_pc_ip(void) {
    app_manager_set_return_to(APP_ID_SETTINGS);
    app_manager_launch(APP_ID_IP_INPUT);
}
static void enter_about(void) {
    app_manager_set_return_to(APP_ID_SETTINGS);
    app_manager_launch(APP_ID_ABOUT);
}
static const char *get_theme_val(void) {
    return theme_get_name(theme_get_base());
}
static void enter_theme(void) {
    app_manager_set_return_to(APP_ID_SETTINGS);
    app_manager_launch(APP_ID_THEME);
}
static void enter_calib(void) {
    app_manager_set_return_to(APP_ID_SETTINGS);
    app_manager_launch(APP_ID_CALIB);
}
static void enter_location(void) {
    s_sub_mode = 4;
    int idx = weather_get_location_index();
    s_sub_val = idx;
    int n = weather_get_location_count();
    const weather_loc_entry_t *locs = weather_get_locations();
    for (int i = 0; i < n && i < 16; i++) s_loc_rows[i] = NULL;

    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    status_bar_create(scr);

    /* 标题（同占卜模块） */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "选择天气地区");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

    /* 滚动容器（同菜单模块） */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 240, 196);
    lv_obj_set_pos(cont, 0, 56);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_ON);

    /* 行放在容器内（同占卜模块样式 + 可滚动） */
    for (int i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_create(cont);
        lv_obj_set_size(row, 240, 28);
        lv_obj_set_pos(row, 0, i * 28);
        lv_obj_set_style_bg_color(row, (i == idx) ? COLOR_ORANGE : COLOR_ROW, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, COLOR_LINE, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, locs[i].name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(lbl, 12, 6);
        s_loc_rows[i] = row;
    }

    /* 初始滚动到选中项 */
    int sy = idx * 28 - 80;
    if (sy < 0) sy = 0;
    lv_obj_scroll_to_y(cont, sy, LV_ANIM_OFF);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
}

/* ================================================================
 *  Value getters for list display
 * ================================================================ */
static char g_val_buf[32];

static const char *get_vol_val(void) {
    settings_t s; settings_load(&s);
    snprintf(g_val_buf, sizeof(g_val_buf), "%d%%", s.volume);
    return g_val_buf;
}
static const char *get_bright_val(void) {
    settings_t s; settings_load(&s);
    snprintf(g_val_buf, sizeof(g_val_buf), "%d%%", s.brightness);
    return g_val_buf;
}
static const char *get_wifi_val(void) {
    if (wifi_manager_is_connected()) {
        snprintf(g_val_buf, sizeof(g_val_buf), "%s", wifi_manager_get_ssid());
    } else {
        snprintf(g_val_buf, sizeof(g_val_buf), "未设置");
    }
    return g_val_buf;
}
static const char *get_sleep_val(void) {
    settings_t s; settings_load(&s);
    snprintf(g_val_buf, sizeof(g_val_buf), "%s %d秒",
             s.sleep_enabled ? "开启" : "关闭", s.sleep_timeout_sec);
    return g_val_buf;
}
static const char *get_pc_ip_val(void) {
    const char *ip = wifi_audio_get_pc_ip();
    return (ip && ip[0]) ? ip : "未设置";
}
static const char *get_about_val(void) {
    return "v1.0";
}
static const char *get_calib_val(void) {
    return "";
}
static const char *get_location_val(void) {
    const char *name = weather_get_location_name();
    return (name && name[0]) ? name : "泉州市区";
}

static const settings_item_t items[MAX_ITEMS] = {
    { "音量",     get_vol_val,     enter_volume },
    { "亮度",     get_bright_val,  enter_brightness },
    { "WiFi配网", get_wifi_val,    enter_wifi },
    { "电脑IP",   get_pc_ip_val,   enter_pc_ip },
    { "天气地区", get_location_val, enter_location },
    { "睡眠",     get_sleep_val,   enter_sleep },
    { "主题",     get_theme_val,   enter_theme },
    { "IMU校准",  get_calib_val,   enter_calib },
    { "关于",     get_about_val,   enter_about },
};

static void update_highlight(lv_obj_t *scr) {
    (void)scr;
    if (!s_list_cont) return;
    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *row = lv_obj_get_child(s_list_cont, i);
        if (!row) continue;
        lv_obj_set_style_bg_color(row,
            (i == s_sel) ? COLOR_ORANGE : COLOR_ROW, 0);
    }
    /* 滚动到选中行 */
    lv_obj_t *sel = lv_obj_get_child(s_list_cont, s_sel);
    if (sel) lv_obj_scroll_to_view(sel, LV_ANIM_ON);
}

static void row_click_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_ITEMS && items[idx].on_enter) {
        items[idx].on_enter();
    }
}

/* ================================================================
 *  Screen creation
 * ================================================================ */
lv_obj_t *settings_screen_create(const settings_t *s) {
    (void)s;
    s_sub_mode = 0;
    s_list_cont = NULL;
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "设置");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

    /* 滚动容器 — 8个item高240px, 容器只显示约7个 */
    s_list_cont = lv_obj_create(scr);
    lv_obj_set_size(s_list_cont, 240, 210);
    lv_obj_set_pos(s_list_cont, 0, 58);
    lv_obj_set_style_bg_opa(s_list_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_list_cont, 0, 0);
    lv_obj_set_style_pad_all(s_list_cont, 0, 0);
    lv_obj_set_style_radius(s_list_cont, 0, 0);
    lv_obj_set_style_clip_corner(s_list_cont, true, 0);
    lv_obj_set_scroll_dir(s_list_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list_cont, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *row = lv_obj_create(s_list_cont);
        lv_obj_set_size(row, 240, ITEM_H);
        lv_obj_set_pos(row, 0, i * ITEM_H);
        lv_obj_set_style_bg_color(row, COLOR_ROW, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, items[i].name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(lbl, 12, (ITEM_H - 16) / 2);

        if (items[i].get_val) {
            lv_obj_t *val = lv_label_create(row);
            lv_label_set_text(val, items[i].get_val());
            lv_obj_set_style_text_color(val, COLOR_GREY, 0);
            lv_obj_set_style_text_font(val, &lv_font_simsun_16_cjk, 0);
            lv_obj_set_pos(val, 112, (ITEM_H - 16) / 2);
        }

        lv_obj_t *arrow = lv_label_create(row);
        lv_label_set_text(arrow, ">");
        lv_obj_set_style_text_color(arrow, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(arrow, 225, (ITEM_H - 16) / 2);

        lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    update_highlight(scr);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

/* ================================================================
 *  Navigation & selection
 * ================================================================ */
void settings_screen_navigate(int dir) {
    if (s_sub_mode > 0) {
        /* UP=增大, DOWN=减小 (key_handler传: UP=-1, DOWN=+1) */
        int delta = -dir;
        switch (s_sub_mode) {
            case 1: case 2:
                s_sub_val += delta * 5;
                if (s_sub_val < 0) s_sub_val = 0;
                if (s_sub_val > 100) s_sub_val = 100;
                break;
            case 3:
                s_sub_val += delta * 10;
                if (s_sub_val < 10) s_sub_val = 10;
                if (s_sub_val > 300) s_sub_val = 300;
                break;
            case 4: {
                int n = weather_get_location_count();
                int old = s_sub_val;
                s_sub_val += dir;  /* UP=-1 → index减小（上移），仿 fortune_screen_navigate */
                if (s_sub_val < 0) s_sub_val = 0;
                if (s_sub_val >= n) s_sub_val = n - 1;
                if (s_sub_val != old) {
                    for (int i = 0; i < n; i++) {
                        if (s_loc_rows[i]) {
                            lv_obj_set_style_bg_color(s_loc_rows[i],
                                (i == s_sub_val) ? COLOR_ORANGE : COLOR_ROW, 0);
                        }
                    }
                    if (s_loc_rows[s_sub_val]) {
                        lv_obj_scroll_to_view(s_loc_rows[s_sub_val], LV_ANIM_ON);
                    }
                }
                break;
            }
        }
        update_sub_display();
        return;
    }

    int old = s_sel;
    s_sel += dir;
    if (s_sel < 0) s_sel = 0;
    if (s_sel >= MAX_ITEMS) s_sel = MAX_ITEMS - 1;
    if (s_sel != old) {
        lv_obj_t *scr = lv_scr_act();
        if (scr) update_highlight(scr);
    }
}

void settings_screen_select(void) {
    if (s_sub_mode > 0) {
        switch (s_sub_mode) {
            case 1: { /* 音量保存 */
                settings_t s; settings_load(&s);
                s.volume = s_sub_val;
                settings_save(&s);
                box_audio_set_volume(s_sub_val);
                recreate_list();
                return;
            }
            case 2: { /* 亮度保存 */
                settings_t s; settings_load(&s);
                s.brightness = s_sub_val;
                settings_save(&s);
                bsp_lcd_backlight_set(s_sub_val);
                recreate_list();
                return;
            }
            case 3: /* 睡眠: A切换开/关 */
                s_sub_bool = !s_sub_bool;
                update_sub_display();
                return;
            case 4: /* 天气地区：保存选择并刷新天气 */
                weather_set_location_by_index(s_sub_val);
                weather_request_refresh();
                recreate_list();
                return;
        }
        return;
    }

    if (s_sel >= 0 && s_sel < MAX_ITEMS && items[s_sel].on_enter) {
        items[s_sel].on_enter();
    }
}

bool settings_screen_back(void) {
    if (s_sub_mode > 0) {
        if (s_sub_mode == 3) {
            /* 睡眠: B退出时保存 */
            settings_t s;
            settings_load(&s);
            s.sleep_enabled = s_sub_bool;
            s.sleep_timeout_sec = s_sub_val;
            settings_save(&s);
        }
        recreate_list();
        return true;
    }
    return false;
}

void settings_screen_update(const settings_t *s) {
    (void)s;
}
