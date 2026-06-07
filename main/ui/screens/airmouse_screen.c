#include "airmouse_screen.h"
#include "modules/pc_remote/ble_hid.h"
#include <stdio.h>
#include <string.h>

#define BG       lv_color_hex(0x0A0A0A)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREY_BG  lv_color_hex(0x333333)

static lv_obj_t *conn_status_lbl, *conn_dot;
static lv_obj_t *sens_btns[3], *map_btns[3];
static lv_obj_t *start_btn_lbl;
static bool ble_connected = false;
static int cur_sens = 1, cur_map = 0;

static const char *sens_names[] = {
    "\xE4\xBD\x8E",       /* 低 */
    "\xE4\xB8\xAD",       /* 中 */
    "\xE9\xAB\x98",       /* 高 */
};
static const char *map_names[] = {
    "\xE9\xBB\x98\xE8\xAE\xA4",   /* 默认 */
    "\xE6\xB8\xB8\xE6\x88\x8F",   /* 游戏 */
    "\xE5\x8A\x9E\xE5\x85\xAC",   /* 办公 */
};

static void update_sens_highlight(void) {
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(sens_btns[i], i == cur_sens ? ORANGE : GREY_BG, 0);
    }
}
static void update_map_highlight(void) {
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(map_btns[i], i == cur_map ? ORANGE : GREY_BG, 0);
    }
}
static void update_conn_display(void) {
    if (ble_connected) {
        lv_label_set_text(conn_status_lbl, "\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"); /* 已连接 */
        lv_obj_set_style_text_color(conn_status_lbl, lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0x4CAF50), 0);
    } else {
        lv_label_set_text(conn_status_lbl, "\xE6\x9C\xAA\xE8\xBF\x9E\xE6\x8E\xA5"); /* 未连接 */
        lv_obj_set_style_text_color(conn_status_lbl, lv_color_hex(0x888888), 0);
        lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0x888888), 0);
    }
}

static void sens_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    cur_sens = i;
    update_sens_highlight();
}
static void map_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    cur_map = i;
    update_map_highlight();
}
static void start_cb(lv_event_t *e) {
    /* Called when user presses "开始控制"
       The app_manager should handle the mode switch */
    extern void app_manager_return(void);
    app_manager_return();
}

lv_obj_t *airmouse_screen_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);

    /* Status bar */
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 240, 28);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, BG, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_t *wifi = lv_label_create(sb);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_pos(wifi, 12, 6);
    lv_obj_t *bat = lv_label_create(sb);
    lv_label_set_text(bat, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(bat, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_pos(bat, 210, 6);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "\xE9\x94\xAE\xE9\xBC\xA0\xE8\xAE\xBE\xE7\xBD\xAE"); /* 键鼠设置 */
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 15, 34);

    /* ── Connection status ── */
    lv_obj_t *conn_icon = lv_label_create(scr);
    lv_label_set_text(conn_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(conn_icon, ORANGE, 0);
    lv_obj_set_pos(conn_icon, 16, 68);

    lv_obj_t *conn_name = lv_label_create(scr);
    lv_label_set_text(conn_name, "\xE7\xA9\xBA\xE4\xB8\xAD\xE9\xA3\x9E\xE9\xBC\xA0"); /* 空中飞鼠 */
    lv_obj_set_style_text_color(conn_name, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_style_text_font(conn_name, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(conn_name, 44, 70);

    conn_dot = lv_obj_create(scr);
    lv_obj_set_size(conn_dot, 8, 8);
    lv_obj_set_pos(conn_dot, 150, 74);
    lv_obj_set_style_bg_color(conn_dot, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_border_width(conn_dot, 0, 0);
    lv_obj_set_style_radius(conn_dot, 4, 0);

    conn_status_lbl = lv_label_create(scr);
    lv_label_set_text(conn_status_lbl, "\xE5\xB7\xB2\xE8\xBF\x9E\xE6\x8E\xA5"); /* 已连接 */
    lv_obj_set_style_text_color(conn_status_lbl, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_text_font(conn_status_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(conn_status_lbl, 165, 70);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 208, 1);
    lv_obj_set_pos(sep, 16, 94);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0xFFFFFF15), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    /* ── Sensitivity ── */
    lv_obj_t *sens_title = lv_label_create(scr);
    lv_label_set_text(sens_title, "\xE7\x81\xB5\xE6\x95\x8F\xE5\xBA\xA6"); /* 灵敏度 */
    lv_obj_set_style_text_color(sens_title, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_style_text_font(sens_title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(sens_title, 16, 108);

    for (int i = 0; i < 3; i++) {
        sens_btns[i] = lv_btn_create(scr);
        lv_obj_set_size(sens_btns[i], 48, 28);
        lv_obj_set_pos(sens_btns[i], 16 + i * 58, 132);
        lv_obj_set_style_bg_color(sens_btns[i], i == 1 ? ORANGE : GREY_BG, 0);
        lv_obj_set_style_radius(sens_btns[i], 8, 0);
        lv_obj_set_style_border_width(sens_btns[i], 0, 0);
        lv_obj_set_style_shadow_width(sens_btns[i], 0, 0);
        lv_obj_t *l = lv_label_create(sens_btns[i]);
        lv_label_set_text(l, sens_names[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFFFF), 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(sens_btns[i], sens_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* ── Key Mapping ── */
    lv_obj_t *map_title = lv_label_create(scr);
    lv_label_set_text(map_title, "\xE6\x8C\x89\xE9\x94\xAE\xE6\x98\xA0\xE5\xB0\x84"); /* 按键映射 */
    lv_obj_set_style_text_color(map_title, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_style_text_font(map_title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(map_title, 16, 172);

    for (int i = 0; i < 3; i++) {
        map_btns[i] = lv_btn_create(scr);
        lv_obj_set_size(map_btns[i], 58, 28);
        lv_obj_set_pos(map_btns[i], 16 + i * 68, 196);
        lv_obj_set_style_bg_color(map_btns[i], i == 0 ? ORANGE : GREY_BG, 0);
        lv_obj_set_style_radius(map_btns[i], 8, 0);
        lv_obj_set_style_border_width(map_btns[i], 0, 0);
        lv_obj_set_style_shadow_width(map_btns[i], 0, 0);
        lv_obj_t *l = lv_label_create(map_btns[i]);
        lv_label_set_text(l, map_names[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFFFF), 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(map_btns[i], map_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* ── Start Control button ── */
    lv_obj_t *start_btn = lv_btn_create(scr);
    lv_obj_set_size(start_btn, 200, 44);
    lv_obj_set_pos(start_btn, 20, 236);
    lv_obj_set_style_bg_color(start_btn, ORANGE, 0);
    lv_obj_set_style_radius(start_btn, 10, 0);
    lv_obj_set_style_border_width(start_btn, 0, 0);
    lv_obj_set_style_shadow_width(start_btn, 0, 0);

    start_btn_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_btn_lbl, "\xF0\x9F\x9A\x80 \xE5\xBC\x80\xE5\xA7\x8B\xE6\x8E\xA7\xE5\x88\xB6"); /* 🚀 开始控制 */
    lv_obj_set_style_text_color(start_btn_lbl, lv_color_hex(0xFFFFFFFF), 0);
    lv_obj_set_style_text_font(start_btn_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(start_btn_lbl);
    lv_obj_add_event_cb(start_btn, start_cb, LV_EVENT_CLICKED, NULL);

    lv_scr_load(scr);
    return scr;
}

/* ── Public API ── */
void airmouse_screen_set_connection(bool connected) {
    ble_connected = connected;
    update_conn_display();
}

void airmouse_screen_set_sensitivity(int level) {
    if (level >= 0 && level < 3) { cur_sens = level; update_sens_highlight(); }
}

void airmouse_screen_set_preset(int preset) {
    if (preset >= 0 && preset < 3) { cur_map = preset; update_map_highlight(); }
}
