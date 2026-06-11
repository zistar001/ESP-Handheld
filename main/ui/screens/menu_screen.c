#include "menu_screen.h"
#include "app/app_manager.h"
#include <stdlib.h>

#define CARD_COLOR    lv_color_hex(0xFF5C00)
#define CARD_W        96
#define CARD_H        68
#define CARD_R        12
#define GRID_COLS     2
#define MAX_ITEMS     10

static lv_obj_t *cards[MAX_ITEMS];
static lv_obj_t *highlight;
static int sel = 0;
static int item_count = 0;

static const struct { const char *icon; const char *label; app_id_t app; } items[] = {
    { LV_SYMBOL_PLAY,    "NES",      APP_ID_NES },
    { "\xE2\x8C\xA8",    "Kbd",      APP_ID_KEYBOARD },
    { LV_SYMBOL_GPS,     "Mouse",    APP_ID_MOUSE },
    { LV_SYMBOL_CLOSE,   "Timer",    APP_ID_COUNTDOWN },
    { LV_SYMBOL_OK,      "Coming",   APP_ID_FORTUNE },
    { LV_SYMBOL_OK,      "Coming",   APP_ID_RECORDER },
    { LV_SYMBOL_SETTINGS,"Config",   APP_ID_SETTINGS },
};

static void card_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= 0 && i < 4) app_manager_launch(items[i].app);
}

lv_obj_t *menu_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A0A), 0);

    /* Status bar */
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 240, 28);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x0A0A0A), 0);
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

    item_count = sizeof(items) / sizeof(items[0]);
    if (item_count > MAX_ITEMS) item_count = MAX_ITEMS;

    int col_x[2] = {16, 128};
    int gap = 16;

    for (int i = 0; i < item_count; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int y = 55 + row * (CARD_H + gap);

        lv_obj_t *card = lv_btn_create(scr);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, col_x[col], y);
        lv_obj_set_style_bg_color(card, CARD_COLOR, 0);
        lv_obj_set_style_radius(card, CARD_R, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_shadow_width(card, 0, 0);

        /* Vertical layout inside button */
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, 4, 0);

        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFFFF), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);

        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, items[i].label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

        lv_obj_add_event_cb(card, card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        cards[i] = card;
    }

    /* Selection highlight */
    sel = 0;
    highlight = lv_obj_create(scr);
    lv_obj_set_size(highlight, CARD_W + 4, CARD_H + 4);
    lv_obj_set_style_border_color(highlight, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(highlight, 2, 0);
    lv_obj_set_style_border_opa(highlight, LV_OPA_50, 0);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_0, 0);
    lv_obj_set_pos(highlight, col_x[0] - 2, 55 - 2);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void menu_screen_navigate(int dx, int dy) {
    int old = sel;
    if (dx != 0) {
        int nx = sel + dx;
        if (nx >= 0 && nx < item_count) sel = nx;
    } else if (dy != 0) {
        int ny = sel + dy * GRID_COLS;
        if (ny >= 0 && ny < item_count) sel = ny;
    }
    if (sel == old) return;

    int col = sel % GRID_COLS;
    int row = sel / GRID_COLS;
    int col_x[2] = {16, 128};
    int gap = 16;

    lv_obj_set_pos(highlight, col_x[col] - 2, 55 + row * (CARD_H + gap) - 2);
}

void menu_screen_select(void) {
    if (sel >= 0 && sel < item_count) {
        app_manager_launch(items[sel].app);
    }
}
