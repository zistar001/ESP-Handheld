#include "settings_screen.h"
#include "app/app_manager.h"
#include "bsp_board.h"
#include "modules/settings/settings_manager.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "modules/pc_remote/wifi_audio.h"
#include "ui/components/status_bar.h"
#include "es8311_driver.h"
#include "modules/audio/box_audio_codec.h"
#include <stdio.h>
#include <string.h>

/* Styling */
#define COLOR_BG     lv_color_hex(0x0A0A0A)
#define COLOR_ROW    lv_color_hex(0x151515)
#define COLOR_ORANGE lv_color_hex(0xFF5C00)
#define COLOR_GREY   lv_color_hex(0x666666)
#define COLOR_LINE   lv_color_hex(0x2A2A2A)

#define ITEM_H 30
#define MAX_ITEMS 7

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
static void enter_calib(void) {
    app_manager_set_return_to(APP_ID_SETTINGS);
    app_manager_launch(APP_ID_CALIB);
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

static const settings_item_t items[MAX_ITEMS] = {
    { "音量",     get_vol_val,     enter_volume },
    { "亮度",     get_bright_val,  enter_brightness },
    { "WiFi配网", get_wifi_val,    enter_wifi },
    { "电脑IP",   get_pc_ip_val,   enter_pc_ip },
    { "睡眠",     get_sleep_val,   enter_sleep },
    { "IMU校准",  get_calib_val,   enter_calib },
    { "关于",     get_about_val,   enter_about },
};

static void update_highlight(lv_obj_t *scr) {
    /* Children: [0]=status_bar, [1]=title, [2..]=rows */
    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *row = lv_obj_get_child(scr, i + 2);
        if (!row) continue;
        lv_obj_set_style_bg_color(row,
            (i == s_sel) ? COLOR_ORANGE : COLOR_ROW, 0);
    }
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

    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 240, ITEM_H);
        lv_obj_set_pos(row, 0, 63 + i * ITEM_H);
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
            lv_obj_set_pos(val, 112, (ITEM_H - 16) / 2);  /* 左移20% (160-48=112) */
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
        if (s_sub_mode == 3) {
            /* 睡眠: A切换开/关 */
            s_sub_bool = !s_sub_bool;
            update_sub_display();
            return;
        }
        settings_t s;
        settings_load(&s);
        switch (s_sub_mode) {
            case 1:
                s.volume = s_sub_val;
                settings_save(&s);
                box_audio_set_volume(s_sub_val);
                break;
            case 2:
                s.brightness = s_sub_val;
                settings_save(&s);
                bsp_lcd_backlight_set(s_sub_val);
                break;
        }
        recreate_list();
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
