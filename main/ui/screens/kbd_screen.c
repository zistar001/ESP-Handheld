#include "kbd_screen.h"
#include "modules/pc_remote/ble_hid.h"
#include <stdio.h>
#include <string.h>

#define BG       lv_color_hex(0x0A0A0A)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREY     lv_color_hex(0x333333)
#define WHITE    lv_color_hex(0xFFFFFF)

static lv_obj_t *toggle_btn;
static bool kb_enabled = false;

bool kbd_screen_is_enabled(void) { return kb_enabled; }

static void update_display(void) {
    lv_obj_set_style_bg_color(toggle_btn, kb_enabled ? ORANGE : GREY, 0);
    lv_obj_t *lbl = lv_obj_get_child(toggle_btn, 0);
    if (lbl) lv_label_set_text(lbl, kb_enabled ? "Kbd: ON" : "Kbd: OFF");
}

lv_obj_t *kbd_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Keyboard");
    lv_obj_set_style_text_color(title, WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(title, 15, 20);

    /* Toggle button */
    toggle_btn = lv_btn_create(scr);
    lv_obj_set_size(toggle_btn, 200, 40);
    lv_obj_set_pos(toggle_btn, 20, 55);
    lv_obj_set_style_radius(toggle_btn, 8, 0);
    lv_obj_t *lbl = lv_label_create(toggle_btn);
    lv_label_set_text(lbl, "Kbd: OFF");
    lv_obj_set_style_text_color(lbl, WHITE, 0);
    lv_obj_center(lbl);

    /* Instructions */
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info,
        "Key Map:\n"
        "A = Enter\n"
        "A hold = Voice\n"
        "B = ESC/Back\n"
        "Arrows = Move\n"
        "\n"
        "START+B = Exit");
    lv_obj_set_style_text_color(info, lv_color_hex(0x999999), 0);
    lv_obj_set_pos(info, 20, 120);

    kb_enabled = false;
    update_display();

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void kbd_screen_navigate(int dy) { /* only one control */ }

void kbd_screen_select(void) {
    kb_enabled = !kb_enabled;
    update_display();
}
