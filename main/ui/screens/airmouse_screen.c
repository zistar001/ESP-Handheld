#include "airmouse_screen.h"
#include "modules/pc_remote/ble_hid.h"
#include "modules/pc_remote/air_mouse.h"
#include "ui/components/status_bar.h"
#include "ui/components/theme_colors.h"
#include <stdio.h>
#include <string.h>

#define BG       CLR_BG
#define ACCENT   CLR_ACCENT
#define BTN_OFF  lv_color_hex(0x333333)
#define WHITE    CLR_TEXT
#define SUB      CLR_SUBTEXT

static lv_obj_t *toggle_btn, *sens_btns[3];
static bool mouse_on = false;
static int cur_sens = 1; /* 0=Low, 1=Mid, 2=High */
static int sel = 0;       /* 0=toggle, 1-3=sens */

bool airmouse_screen_mouse_enabled(void) { return mouse_on; }

static void update_sens(void) {
    float vals[] = {0.25f, 0.5f, 1.0f};
    air_mouse_set_sensitivity(vals[cur_sens]);
}

static void update_display(void) {
    /* Toggle */
    lv_obj_set_style_bg_color(toggle_btn, mouse_on ? ACCENT : BTN_OFF, 0);
    lv_obj_t *lbl = lv_obj_get_child(toggle_btn, 0);
    if (lbl) {
        lv_label_set_text(lbl, mouse_on ? "鼠标: 开" : "鼠标: 关");
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
    }
    lv_obj_set_style_border_width(toggle_btn, sel == 0 ? 2 : 0, 0);
    lv_obj_set_style_border_color(toggle_btn, WHITE, 0);

    /* Sensitivity buttons */
    for (int i = 0; i < 3; i++) {
        bool active = (i == cur_sens);
        lv_obj_set_style_bg_color(sens_btns[i], active ? ACCENT : BTN_OFF, 0);
        lv_obj_set_style_border_width(sens_btns[i], (sel == i + 1) ? 2 : 0, 0);
        lv_obj_set_style_border_color(sens_btns[i], WHITE, 0);
    }
}

lv_obj_t *airmouse_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* Status bar */
    status_bar_create(scr);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "鼠标");
    lv_obj_set_style_text_color(title, WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 15, 36);

    /* Toggle */
    toggle_btn = lv_btn_create(scr);
    lv_obj_set_size(toggle_btn, 200, 40);
    lv_obj_set_pos(toggle_btn, 20, 71);
    lv_obj_set_style_radius(toggle_btn, 8, 0);
    lv_obj_t *lbl = lv_label_create(toggle_btn);
    lv_label_set_text(lbl, "鼠标: 关");
    lv_obj_set_style_text_color(lbl, WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(lbl);

    /* Sensitivity label */
    lv_obj_t *sens_lbl = lv_label_create(scr);
    lv_label_set_text(sens_lbl, "灵敏度:");
    lv_obj_set_style_text_color(sens_lbl, SUB, 0);
    lv_obj_set_style_text_font(sens_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(sens_lbl, 20, 110);

    /* Sensitivity buttons */
    const char *names[] = {"低", "中", "高"};
    for (int i = 0; i < 3; i++) {
        sens_btns[i] = lv_btn_create(scr);
        lv_obj_set_size(sens_btns[i], 58, 32);
        lv_obj_set_pos(sens_btns[i], 20 + i * 66, 132);
        lv_obj_set_style_radius(sens_btns[i], 8, 0);
        lv_obj_t *sl = lv_label_create(sens_btns[i]);
        lv_label_set_text(sl, names[i]);
        lv_obj_set_style_text_color(sl, WHITE, 0);
        lv_obj_set_style_text_font(sl, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(sl);
    }

    /* Instructions */
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info,
        "按键说明:\n"
        "A = 左键\n"
        "B = 右键\n"
        "\xE2\x86\x92 = 语音\n"
        "START+B = 退出");
    lv_obj_set_style_text_color(info, SUB, 0);
    lv_obj_set_style_text_font(info, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(info, 20, 185);

    /* Init state */
    mouse_on = false;
    cur_sens = 1;
    sel = 0;
    update_sens();
    update_display();

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void airmouse_screen_navigate(int dx, int dy) {
    (void)dx;
    int old = sel;
    if (dy > 0) sel = (sel < 3) ? sel + 1 : 3;
    if (dy < 0) sel = (sel > 0) ? sel - 1 : 0;
    if (sel != old) update_display();
}

void airmouse_screen_select(void) {
    if (sel == 0) {
        mouse_on = !mouse_on;
    } else if (sel >= 1 && sel <= 3) {
        cur_sens = sel - 1;
        update_sens();
    }
    update_display();
}
