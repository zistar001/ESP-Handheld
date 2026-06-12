#include "liuren_screen.h"
#include "ui/components/status_bar.h"
#include <stdio.h>
#include "esp_random.h"

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_WHITE    lv_color_hex(0xFFFFFF)
#define COLOR_GREY     lv_color_hex(0x999999)
#define COLOR_ORANGE   lv_color_hex(0xFF5C00)

static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_frames[6];
static lv_obj_t *s_labels[6];
static lv_obj_t *s_title_lbl;
static lv_obj_t *s_order_lbl;
static lv_obj_t *s_desc1, *s_desc2, *s_desc3;
static lv_obj_t *s_palm_lbl;
static lv_obj_t *s_result_lbl;

static const char *liuren_names[] = {"\xe5\xa4\xa7\xe5\xae\x89", "\xe7\x95\x99\xe8\xbf\x9e", "\xe9\x80\x9f\xe5\x96\x9c", "\xe8\xb5\x9b\xe5\x8f\xa3", "\xe5\xb0\x8f\xe5\x90\x89", "\xe7\xa9\xba\xe4\xba\xa1"}; /* 大安 留连 速喜 赤口 小吉 空亡 */
static const uint32_t liuren_colors[] = {0x1A1A2E, 0x2A1A1E, 0x1A2E1A, 0x2E1A1A, 0x2E2E1A, 0x1A2E2E};
static const char *liuren_desc[] = {"\xe5\xa4\xa7\xe5\x90\x89", "\xe6\x8b\x96\xe5\xbb\xb6", "\xe5\x96\x9c\xe4\xba\x8b", "\xe4\xba\x89\xe6\x89\xa7", "\xe5\xa5\xbd\xe7\xbb\x93\xe6\x9e\x9c", "\xe5\xa4\xa7\xe5\x88\xa9"}; /* 大吉 拖延 喜事 争执 好结果 大利 */

/* Grid positions for 6 frames: 3 columns x 2 rows */
static const int frame_x[] = {30, 95, 160};
static const int frame_y[] = {70, 130};

static void frame_click_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= 6) return;

    /* Highlight selected frame */
    for (int i = 0; i < 6; i++) {
        lv_obj_set_style_border_width(s_frames[i], 0, 0);
    }
    lv_obj_set_style_border_color(s_frames[idx], COLOR_ORANGE, 0);
    lv_obj_set_style_border_width(s_frames[idx], 2, 0);

    /* Show result */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %s", liuren_names[idx], liuren_desc[idx]);
    lv_label_set_text(s_result_lbl, buf);
    lv_obj_clear_flag(s_result_lbl, LV_OBJ_FLAG_HIDDEN);
}

static void shuffle_cb(lv_event_t *e) {
    (void)e;
    /* Randomly select one of 6 positions */
    int idx = esp_random() % 6;

    /* Animate: flash through positions */
    for (int i = 0; i < 6; i++) {
        lv_obj_set_style_border_width(s_frames[i], 0, 0);
    }
    lv_obj_set_style_border_color(s_frames[idx], COLOR_ORANGE, 0);
    lv_obj_set_style_border_width(s_frames[idx], 2, 0);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %s", liuren_names[idx], liuren_desc[idx]);
    lv_label_set_text(s_result_lbl, buf);
    lv_obj_clear_flag(s_result_lbl, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *liuren_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COLOR_BG, 0);

    /* Status bar */
    status_bar_create(s_scr);

    /* Title */
    s_title_lbl = lv_label_create(s_scr);
    lv_label_set_text(s_title_lbl, "\xe8\xbf\x90\xe5\x8a\xbf"); /* 运势 */
    lv_obj_set_style_text_color(s_title_lbl, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(s_title_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_title_lbl, 10, 34);

    /* 6 position frames: 3 columns x 2 rows */
    int idx = 0;
    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 2; row++) {
            int i = idx++;
            lv_obj_t *f = lv_obj_create(s_scr);
            lv_obj_set_size(f, 50, 60);
            lv_obj_set_pos(f, frame_x[col], frame_y[row]);
            lv_obj_set_style_bg_color(f, lv_color_hex(liuren_colors[i]), 0);
            lv_obj_set_style_radius(f, 6, 0);
            lv_obj_set_style_border_width(f, 0, 0);
            lv_obj_set_style_pad_all(f, 0, 0);
            lv_obj_add_flag(f, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(f, frame_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            s_frames[i] = f;

            lv_obj_t *lbl = lv_label_create(f);
            lv_label_set_text(lbl, liuren_names[i]);
            lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
            lv_obj_set_style_text_font(lbl, &lv_font_simsun_16_cjk, 0);
            lv_obj_center(lbl);
            s_labels[i] = lbl;
        }
    }

    /* Palm label */
    s_palm_lbl = lv_label_create(s_scr);
    lv_label_set_text(s_palm_lbl, "食指    中指    无名指");
    lv_obj_set_style_text_color(s_palm_lbl, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(s_palm_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_palm_lbl, 30, 200);

    /* Order */
    s_order_lbl = lv_label_create(s_scr);
    lv_label_set_text(s_order_lbl, "大安>留连>速喜>赤口>小吉>空亡");
    lv_obj_set_style_text_color(s_order_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(s_order_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_order_lbl, 10, 225);

    /* Descriptions */
    s_desc1 = lv_label_create(s_scr);
    lv_label_set_text(s_desc1, "大安:大吉  留连:拖延");
    lv_obj_set_style_text_color(s_desc1, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc1, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_desc1, 10, 238);

    s_desc2 = lv_label_create(s_scr);
    lv_label_set_text(s_desc2, "速喜:喜事  赤口:争执");
    lv_obj_set_style_text_color(s_desc2, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc2, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_desc2, 10, 250);

    s_desc3 = lv_label_create(s_scr);
    lv_label_set_text(s_desc3, "小吉:好结果  空亡:大利");
    lv_obj_set_style_text_color(s_desc3, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc3, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_desc3, 10, 262);

    /* Result label (hidden until selection) */
    s_result_lbl = lv_label_create(s_scr);
    lv_label_set_text(s_result_lbl, "");
    lv_obj_set_style_text_color(s_result_lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_result_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_result_lbl, 10, 175);
    lv_obj_add_flag(s_result_lbl, LV_OBJ_FLAG_HIDDEN);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    return s_scr;
}
