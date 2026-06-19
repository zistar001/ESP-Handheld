#include "menu_screen.h"
#include "app/app_manager.h"
#include "ui/components/status_bar.h"
#include <stdlib.h>

#define CARD_COLOR    lv_color_hex(0xFF5C00)
#define CARD_W        96
#define CARD_H        68
#define CARD_R        12
#define GRID_COLS     2
#define GAP           12
#define CARD_START_Y  8
#define ICON_SIZE     28

static lv_obj_t *cards[9];
static lv_obj_t *highlight;
static lv_obj_t *scroll_cont;
static int sel;
static int item_count = 8;

#define HIGHLIGHT_COLOR  lv_color_hex(0xFFBB00)   /* Golden yellow — highly visible on dark bg */

/* Pencil design: icon + Chinese label */
static const struct { const char *icon; const char *label; app_id_t app; } items[] = {
    { LV_SYMBOL_PLAY,    "\xe6\xb8\xb8\xe6\x88\x8f",  APP_ID_NES },         /* 游戏 */
    { LV_SYMBOL_EYE_OPEN,"\xe5\xb0\x8f\xe6\x99\xba",  APP_ID_XIAOZHI },    /* 小智 */
    { LV_SYMBOL_KEYBOARD,"\xe9\x94\xae\xe7\x9b\x98",  APP_ID_KEYBOARD },    /* 键盘 */
    { LV_SYMBOL_GPS,     "\xe9\xbc\xa0\xe6\xa0\x87",  APP_ID_MOUSE },       /* 鼠标 */
    { LV_SYMBOL_REFRESH, "\xe5\x8d\xa0\xe5\x8d\x9c",  APP_ID_FORTUNE },     /* 占卜 */
    { LV_SYMBOL_LOOP,    "\xe5\xb0\x8f\xe5\x85\xad\xe5\xa3\xac",  APP_ID_RECORDER },    /* 小六壬 */
    { LV_SYMBOL_BELL,    "\xe8\xae\xa1\xe6\x97\xb6",  APP_ID_COUNTDOWN },   /* 计时 */
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

    /* Reset static pointers — keep sel to remember last selection across visits */
    scroll_cont = NULL;
    highlight = NULL;
    for (int i = 0; i < 9; i++) cards[i] = NULL;

    /* Status bar */
    status_bar_create(scr);

    /* Scrollable container for cards - Pencil: y=28, height=252 */
    scroll_cont = lv_obj_create(scr);
    lv_obj_set_size(scroll_cont, 240, 252);
    lv_obj_set_pos(scroll_cont, 0, 28);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(scroll_cont, 0, 0);
    lv_obj_set_style_radius(scroll_cont, 0, 0);
    lv_obj_set_style_pad_all(scroll_cont, 0, 0);
    lv_obj_set_scroll_dir(scroll_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_cont, LV_SCROLLBAR_MODE_ON);
    /* NOT setting content_height — children beyond 252px bounds enable scroll */

    /* Selection highlight — bright golden border + subtle fill */
    highlight = lv_obj_create(scroll_cont);
    lv_obj_set_size(highlight, CARD_W + 6, CARD_H + 6);
    lv_obj_set_style_border_color(highlight, HIGHLIGHT_COLOR, 0);
    lv_obj_set_style_border_width(highlight, 3, 0);
    lv_obj_set_style_border_opa(highlight, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(highlight, HIGHLIGHT_COLOR, 0);
    lv_obj_set_style_bg_opa(highlight, 18, 0);         /* 7% fill tint */
    lv_obj_set_style_radius(highlight, CARD_R + 2, 0);
    lv_obj_set_style_pad_all(highlight, 0, 0);
    lv_obj_set_style_shadow_color(highlight, HIGHLIGHT_COLOR, 0);
    lv_obj_set_style_shadow_opa(highlight, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(highlight, 12, 0);
    lv_obj_set_style_shadow_spread(highlight, 2, 0);
    lv_obj_set_pos(highlight, (240 - GRID_COLS * CARD_W - GAP) / 2 - 3, CARD_START_Y - 3);

    /* Create cards - Pencil layout: 2 columns */
    for (int i = 0; i < item_count; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int x = (240 - GRID_COLS * CARD_W - GAP) / 2 + col * (CARD_W + GAP);
        int y = CARD_START_Y + row * (CARD_H + GAP);

        lv_obj_t *card = lv_obj_create(scroll_cont);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_style_bg_color(card, CARD_COLOR, 0);
        lv_obj_set_style_radius(card, CARD_R, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* Icon - centered in card (Pencil: 28x28 icon box) */
        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

        /* Label - below icon (Pencil: 14px Noto Sans SC) */
        lv_obj_t *label = lv_label_create(card);
        lv_label_set_text(label, items[i].label);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -8);

        cards[i] = card;
    }

    lv_scr_load(scr);

    /* If returning from an app, restore highlight to remembered position */
    if (sel > 0 && highlight && scroll_cont) {
        int col = sel % GRID_COLS;
        int row = sel / GRID_COLS;
        int x = (240 - GRID_COLS * CARD_W - GAP) / 2 + col * (CARD_W + GAP);
        int y = CARD_START_Y + row * (CARD_H + GAP);
        lv_obj_set_pos(highlight, x - 3, y - 3);
        int sy = y - 40;
        if (sy < 0) sy = 0;
        lv_obj_scroll_to_y(scroll_cont, sy, LV_ANIM_OFF);
    }

    /* Delete old screen AFTER loading new one */
    if (old) lv_obj_del(old);

    return scr;
}

void menu_screen_navigate(int dx, int dy) {
    int old_sel = sel;
    if (dy != 0) {
        int new_sel = sel + dy * GRID_COLS;
        if (new_sel < 0 || new_sel >= item_count) return;  /* At edge — don't wrap */
        sel = new_sel;
    } else if (dx != 0) {
        int new_sel = sel + dx;
        if (new_sel < 0 || new_sel >= item_count) return;  /* At edge — don't wrap */
        sel = new_sel;
    }
    if (sel != old_sel && highlight && scroll_cont) {
        int col = sel % GRID_COLS;
        int row = sel / GRID_COLS;
        int x = (240 - GRID_COLS * CARD_W - GAP) / 2 + col * (CARD_W + GAP);
        int y = CARD_START_Y + row * (CARD_H + GAP);
        lv_obj_set_pos(highlight, x - 3, y - 3);
        int sy = y - 40;
        if (sy < 0) sy = 0;
        lv_obj_scroll_to_y(scroll_cont, sy, LV_ANIM_ON);
    }
}

void menu_screen_select(void) {
    if (sel >= 0 && sel < item_count) {
        app_manager_launch(items[sel].app);
    }
}
