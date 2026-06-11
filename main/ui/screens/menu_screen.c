#include "menu_screen.h"
#include "app/app_manager.h"
#include <stdlib.h>

#define CARD_COLOR    lv_color_hex(0xFF5C00)
#define CARD_W        96
#define CARD_H        68
#define CARD_R        12
#define GRID_COLS     2
#define MAX_ITEMS     10
#define GAP           12
#define TOP_Y         8

static lv_obj_t *cards[MAX_ITEMS];
static lv_obj_t *highlight;
static lv_obj_t *scroll_cont;
static int sel = 0;
static int item_count = 0;

static const struct { const char *icon; const char *label; app_id_t app; } items[] = {
    { LV_SYMBOL_PLAY,    "\xe6\xb8\xb8\xe6\x88\x8f",  APP_ID_NES },         /* 游戏 */
    { "\xE2\x8C\xA8",    "\xe9\x94\xae\xe7\x9b\x98",  APP_ID_KEYBOARD },    /* 键盘 */
    { LV_SYMBOL_GPS,     "\xe9\xbc\xa0\xe6\xa0\x87",  APP_ID_MOUSE },       /* 鼠标 */
    { LV_SYMBOL_CLOSE,   "\xe9\x97\xb9\xe9\x92\x9f",  APP_ID_COUNTDOWN },   /* 闹钟 */
    { LV_SYMBOL_OK,      "\xe8\xbf\x90\xe5\x8a\xbf",  APP_ID_FORTUNE },     /* 运势 */
    { LV_SYMBOL_OK,      "\xe5\xbd\x95\xe9\x9f\xb3",  APP_ID_RECORDER },    /* 录音 */
    { LV_SYMBOL_SETTINGS,"\xe8\xae\xbe\xe7\xbd\xae",  APP_ID_SETTINGS },    /* 设置 */
};

static void card_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= 0 && i < item_count) app_manager_launch(items[i].app);
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

    /* Scrollable container for cards */
    item_count = sizeof(items) / sizeof(items[0]);
    if (item_count > MAX_ITEMS) item_count = MAX_ITEMS;
    int rows = (item_count + GRID_COLS - 1) / GRID_COLS;
    int content_h = TOP_Y + rows * (CARD_H + GAP) + 20;

    scroll_cont = lv_obj_create(scr);
    lv_obj_set_size(scroll_cont, 240, 280 - 28); /* below status bar */
    lv_obj_set_pos(scroll_cont, 0, 28);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(scroll_cont, 0, 0);
    lv_obj_set_style_radius(scroll_cont, 0, 0);
    lv_obj_set_style_pad_all(scroll_cont, 0, 0);
    lv_obj_set_scroll_dir(scroll_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_cont, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_content_height(scroll_cont, content_h);

    /* Selection highlight — inside scroll container */
    highlight = lv_obj_create(scroll_cont);
    lv_obj_set_size(highlight, CARD_W + 4, CARD_H + 4);
    lv_obj_set_style_border_color(highlight, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(highlight, 2, 0);
    lv_obj_set_style_border_opa(highlight, LV_OPA_50, 0);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_0, 0);
    lv_obj_set_pos(highlight, (240 - GRID_COLS * CARD_W - GAP) / 2 - 2, TOP_Y - 2);

    int margin = 10;
    int col_x[2] = { margin, margin + CARD_W + GAP };

    for (int i = 0; i < item_count; i++) {
        int col = i % GRID_COLS, row = i / GRID_COLS;
        int y = TOP_Y + row * (CARD_H + GAP);

        lv_obj_t *card = lv_btn_create(scroll_cont);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, col_x[col], y);
        lv_obj_set_style_bg_color(card, CARD_COLOR, 0);
        lv_obj_set_style_radius(card, CARD_R, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
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

    sel = 0;
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

    int col = sel % GRID_COLS, row = sel / GRID_COLS;
    int margin = 10;
    int col_x[2] = { margin, margin + CARD_W + GAP };
    lv_obj_set_pos(highlight, col_x[col] - 2, TOP_Y + row * (CARD_H + GAP) - 2);

    /* Auto-scroll to show selected item */
    lv_obj_scroll_to_view_recursive(cards[sel], LV_ANIM_ON);
}

void menu_screen_select(void) {
    if (sel >= 0 && sel < item_count) {
        app_manager_launch(items[sel].app);
    }
}
