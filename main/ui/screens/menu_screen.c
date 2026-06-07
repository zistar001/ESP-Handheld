#include "menu_screen.h"
#include "app/app_manager.h"

#define CARD_COLOR    lv_color_hex(0xFF5C00)
#define CARD_W        96
#define CARD_H        68
#define CARD_R        12
#define GRID_COLS     2

static lv_obj_t *cards[4];
static lv_obj_t *highlight;
static int sel = 0;

static const struct { const char *icon; const char *label; app_id_t app; } items[] = {
    { "\xEF\x84\x9B", "\xE6\xB8\xB8\xE6\x88\x8F", APP_ID_NES },        /* 游戏 */
    { "\xEF\x84\x9C", "\xE9\x94\xAE\xE9\xBC\xA0", APP_ID_PC_REMOTE },  /* 键鼠 */
    { "\xEF\x80\xB3", "\xE9\x97\xB9\xE9\x92\x9F", APP_ID_COUNTDOWN },  /* 闹钟 → 倒计时 */
    { "\xEF\x80\x93", "\xE8\xAE\xBE\xE7\xBD\xAE", APP_ID_SETTINGS },   /* 设置 */
};

static void card_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i >= 0 && i < 4) app_manager_launch(items[i].app);
}

lv_obj_t *menu_screen_create(void) {
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

    /* 2×2 Grid: top edge y=55, bottom edge y=280-30=250 */
    /* Row Y: 55, (250-55-68)/2 = gap between rows...
       Actually 55 + 68 = 123, then gap 16, then 123+16+68=207. Close enough */
    int row1_y = 55, row2_y = 139, col_x[2] = {16, 128};

    for (int i = 0; i < 4; i++) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;
        int y = (row == 0) ? row1_y : row2_y;

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
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);

        lv_obj_add_event_cb(card, card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        cards[i] = card;
    }

    /* Selection highlight */
    highlight = lv_obj_create(scr);
    lv_obj_set_size(highlight, CARD_W + 4, CARD_H + 4);
    lv_obj_set_style_border_color(highlight, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(highlight, 2, 0);
    lv_obj_set_style_border_opa(highlight, LV_OPA_50, 0);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_0, 0);
    lv_obj_set_pos(highlight, col_x[0] - 2, row1_y - 2);

    lv_scr_load(scr);
    return scr;
}

void menu_screen_navigate(int dx, int dy) {
    int old = sel;
    if (dx != 0) {
        int nx = sel + dx;
        if (nx >= 0 && nx < 4) sel = nx;
    } else if (dy != 0) {
        int ny = sel + dy * GRID_COLS;
        if (ny >= 0 && ny < 4) sel = ny;
    }
    if (sel == old) return;

    int row = sel / GRID_COLS;
    int col = sel % GRID_COLS;
    int col_x[2] = {16, 128};
    int row_y[2] = {55, 139};

    lv_obj_set_pos(highlight, col_x[col] - 2, row_y[row] - 2);
}

void menu_screen_select(void) {
    if (sel >= 0 && sel < 4) {
        app_manager_launch(items[sel].app);
    }
}
