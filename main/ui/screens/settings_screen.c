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

#define ITEM_H 36
#define MAX_ITEMS 9

typedef struct {
    const char *name;
    const char *(*get_val)(void);
    void (*on_enter)(void);
} settings_item_t;

/* === Sub-screen state ===
 * s_sub_mode: 0=list, 1=volume, 2=brightness, 3=sleep
 * s_sub_val: current value being adjusted */
static int s_sub_mode = 0;
static int s_sub_val = 0;
static bool s_sub_bool = false;  /* sleep enabled toggle */

/* ================================================================
 *  Sub-screen helpers
 * ================================================================ */

static void recreate_list(void) {
    s_sub_mode = 0;
    settings_t s;
    settings_load(&s);
    settings_screen_create(&s);
}

static void show_sub_screen(const char *title) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    status_bar_create(scr);

    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(t, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(t, 240);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);

    /* Hint at bottom */
    lv_obj_t *h = lv_label_create(scr);
    lv_label_set_text(h, "UP/DOWN: adjust   A: save   B: back");
    lv_obj_set_style_text_color(h, lv_color_hex(0x666666), 0);
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

    /* Remove old value label */
    lv_obj_t *v = lv_obj_get_child(scr, 2);
    if (v) lv_obj_del(v);

    char buf[48];
    lv_obj_t *val = lv_label_create(scr);
    switch (s_sub_mode) {
        case 1: snprintf(buf, sizeof(buf), "Volume: %d%%", s_sub_val); break;
        case 2: snprintf(buf, sizeof(buf), "Brightness: %d%%", s_sub_val); break;
        case 3: snprintf(buf, sizeof(buf), "Sleep: %s   %ds",
                         s_sub_bool ? "ON" : "OFF", s_sub_val); break;
        default: return;
    }
    lv_label_set_text(val, buf);
    lv_obj_set_style_text_color(val, lv_color_hex(0xFF5C00), 0);
    lv_obj_set_style_text_font(val, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(val, 240);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, -10);
}

/* ================================================================
 *  Entry callbacks
 * ================================================================ */
static void enter_volume(void) {
    settings_t s; settings_load(&s);
    s_sub_mode = 1;
    s_sub_val = s.volume;
    show_sub_screen("Volume");
    update_sub_display();
}

static void enter_brightness(void) {
    settings_t s; settings_load(&s);
    s_sub_mode = 2;
    s_sub_val = s.brightness;
    show_sub_screen("Brightness");
    update_sub_display();
}

static void enter_sleep(void) {
    settings_t s; settings_load(&s);
    s_sub_mode = 3;
    s_sub_val = s.sleep_timeout_sec;
    s_sub_bool = s.sleep_enabled;
    show_sub_screen("Sleep Settings");
    update_sub_display();
}

static void enter_wifi(void) {
    app_manager_launch(APP_ID_WIFI_SETUP);
}
static void enter_pc_ip(void) {
    app_manager_launch(APP_ID_IP_INPUT);
}
static void enter_about(void) {
    app_manager_launch(APP_ID_ABOUT);
}
static void enter_calib(void) {
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
        snprintf(g_val_buf, sizeof(g_val_buf), "not set");
    }
    return g_val_buf;
}
static const char *get_sleep_val(void) {
    settings_t s; settings_load(&s);
    snprintf(g_val_buf, sizeof(g_val_buf), "%s %ds",
             s.sleep_enabled ? "ON" : "OFF", s.sleep_timeout_sec);
    return g_val_buf;
}
static const char *get_pc_ip_val(void) {
    const char *ip = wifi_audio_get_pc_ip();
    return (ip && ip[0]) ? ip : "not set";
}
static const char *get_about_val(void) {
    return "v1.0";
}
static const char *get_calib_val(void) {
    return "";
}

static const settings_item_t items[MAX_ITEMS] = {
    { "Volume",     get_vol_val,     enter_volume },
    { "Brightness", get_bright_val,  enter_brightness },
    { "WiFi",       get_wifi_val,    enter_wifi },
    { "PC IP",      get_pc_ip_val,   enter_pc_ip },
    { "Sleep",      get_sleep_val,   enter_sleep },
    { "IMU Calib",  get_calib_val,   enter_calib },
    { "About",      get_about_val,   enter_about },
};

static int s_sel = 0;

static void update_highlight(lv_obj_t *scr) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *row = lv_obj_get_child(scr, i + 1);
        if (!row) continue;
        lv_obj_set_style_bg_color(row,
            (i == s_sel) ? lv_color_hex(0xFF5C00) : lv_color_hex(0x2a2a3e), 0);
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
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Settings (B: Back)");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(title, 10, 36);

    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, 240, ITEM_H);
        lv_obj_set_pos(row, 0, 63 + i * ITEM_H);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, items[i].name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(lbl, 12, (ITEM_H - 14) / 2);

        if (items[i].get_val) {
            lv_obj_t *val = lv_label_create(row);
            lv_label_set_text(val, items[i].get_val());
            lv_obj_set_style_text_color(val, lv_color_hex(0x999999), 0);
            lv_obj_set_style_text_font(val, &lv_font_simsun_16_cjk, 0);
            lv_obj_set_pos(val, 160, (ITEM_H - 14) / 2);
        }

        lv_obj_t *arrow = lv_label_create(row);
        lv_label_set_text(arrow, ">");
        lv_obj_set_style_text_color(arrow, lv_color_hex(0x666666), 0);
        lv_obj_set_pos(arrow, 225, (ITEM_H - 14) / 2);

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
        /* In sub-screen: adjust value */
        switch (s_sub_mode) {
            case 1: /* Volume */
                s_sub_val += dir * 5;
                if (s_sub_val < 0) s_sub_val = 0;
                if (s_sub_val > 100) s_sub_val = 100;
                break;
            case 2: /* Brightness */
                s_sub_val += dir * 5;
                if (s_sub_val < 0) s_sub_val = 0;
                if (s_sub_val > 100) s_sub_val = 100;
                break;
            case 3: /* Sleep timeout */
                s_sub_val += dir * 10;
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
        /* Save current value and return to list */
        settings_t s;
        settings_load(&s);
        switch (s_sub_mode) {
            case 1:
                s.volume = s_sub_val;
                settings_save(&s);
                bsp_lcd_backlight_set(s.brightness); /* no-op, just apply volume */
                box_audio_set_volume(s_sub_val);
                break;
            case 2:
                s.brightness = s_sub_val;
                settings_save(&s);
                bsp_lcd_backlight_set(s_sub_val);
                break;
            case 3:
                s.sleep_enabled = s_sub_bool;
                s.sleep_timeout_sec = s_sub_val;
                settings_save(&s);
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
        /* Exit sub-screen without saving, return to settings list */
        recreate_list();
        return true;
    }
    return false;
}

void settings_screen_update(const settings_t *s) {
    (void)s;
}
