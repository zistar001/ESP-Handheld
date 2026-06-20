#include "kbd_screen.h"
#include "modules/pc_remote/ble_hid.h"
#include "ui/components/status_bar.h"
#include "ui/components/theme_colors.h"
#include <stdio.h>
#include <string.h>

#define BG       CLR_BG
#define ACCENT   CLR_ACCENT
#define GREEN    lv_color_hex(0x4CAF50)
#define BTN_OFF  lv_color_hex(0x333333)
#define WHITE    CLR_TEXT
#define SUB      CLR_SUBTEXT

static lv_obj_t *kbd_btn, *voice_btn;
static bool kb_enabled = false;
static bool voice_active = false;
static int sel = 0;  /* 0=kbd, 1=voice */

bool kbd_screen_is_enabled(void) { return kb_enabled; }

static void update_display(void) {
    /* Kbd toggle */
    lv_obj_set_style_bg_color(kbd_btn, kb_enabled ? ACCENT : BTN_OFF, 0);
    lv_obj_set_style_border_width(kbd_btn, sel == 0 ? 2 : 0, 0);
    lv_obj_set_style_border_color(kbd_btn, WHITE, 0);
    lv_obj_t *lbl = lv_obj_get_child(kbd_btn, 0);
    if (lbl) {
        lv_label_set_text(lbl, kb_enabled ? "键盘: 开" : "键盘: 关");
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
    }

    /* Voice indicator */
    lv_obj_set_style_bg_color(voice_btn, voice_active ? GREEN : BTN_OFF, 0);
    lv_obj_set_style_border_width(voice_btn, sel == 1 ? 2 : 0, 0);
    lv_obj_set_style_border_color(voice_btn, WHITE, 0);
    lbl = lv_obj_get_child(voice_btn, 0);
    if (lbl) {
        lv_label_set_text(lbl, voice_active ? "语音: 开" : "语音: 关");
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
    }
}

lv_obj_t *kbd_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* Status bar */
    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "键盘");
    lv_obj_set_style_text_color(title, WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 15, 36);

    /* Kbd toggle button */
    kbd_btn = lv_btn_create(scr);
    lv_obj_set_size(kbd_btn, 200, 40);
    lv_obj_set_pos(kbd_btn, 20, 71);
    lv_obj_set_style_radius(kbd_btn, 8, 0);
    lv_obj_t *lbl = lv_label_create(kbd_btn);
    lv_label_set_text(lbl, "键盘: 关");
    lv_obj_set_style_text_color(lbl, WHITE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(lbl);

    /* Voice state button */
    voice_btn = lv_btn_create(scr);
    lv_obj_set_size(voice_btn, 200, 40);
    lv_obj_set_pos(voice_btn, 20, 126);
    lv_obj_set_style_radius(voice_btn, 8, 0);
    lv_obj_t *vlbl = lv_label_create(voice_btn);
    lv_label_set_text(vlbl, "语音: 关");
    lv_obj_set_style_text_color(vlbl, WHITE, 0);
    lv_obj_set_style_text_font(vlbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(vlbl);

    /* Instructions */
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info,
        "按键 (键盘开):\n"
        "A = 回车  B = ESC\n"
        "方向键 = 移动\n"
        "按住右键 = 语音(PTT)\n"
        "START+B = 退出");
    lv_obj_set_style_text_color(info, SUB, 0);
    lv_obj_set_style_text_font(info, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(info, 20, 170);

    kb_enabled = false;
    voice_active = false;
    sel = 0;
    update_display();

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void kbd_screen_navigate(int dy) {
    if (dy > 0 && sel < 1) sel++;
    if (dy < 0 && sel > 0) sel--;
    update_display();
}

void kbd_screen_select(void) {
    if (sel == 0) { kb_enabled = !kb_enabled; }
    /* Voice is PTT, not toggled here */
    update_display();
}

void kbd_screen_set_voice_active(bool active) {
    voice_active = active;
    update_display();
}
