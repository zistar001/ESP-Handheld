#include "fortune_screen.h"
#include "ui/components/status_bar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_random.h"

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_ORANGE   lv_color_hex(0xFF5C00)
#define COLOR_WHITE    lv_color_hex(0xFFFFFF)
#define COLOR_GREY     lv_color_hex(0x999999)
#define COLOR_LABEL    lv_color_hex(0x999999)

#define LINE_X         30
#define LINE_W         180
#define LINE_H         10
#define LINE_GAP       18
#define LINE_Y_START   75
#define LABEL_X        210
#define BTN_Y          235
#define BTN_W          180
#define BTN_H          36

static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_lines[6][2];  /* 6 爻, each has up to 2 parts (yang=1, yin=2) */
static lv_obj_t *s_labels[6];
static lv_obj_t *s_btn;
static lv_obj_t *s_btn_label;
static lv_obj_t *s_title_lbl;
static lv_obj_t *s_inst_lbl;
static lv_obj_t *s_result_lbl;

/* 6 爻 data: 1=yang(solid), 0=yin(broken) */
static int s_yao[6] = {1, 1, 1, 0, 1, 0};
static const char *yao_names[] = {"上", "五", "四", "三", "二", "初"};

/* 64 hexagram lookup: lower 3 bits = lower trigram, upper 3 bits = upper trigram */
static const char *hexagram_names[64] = {
    "坤为地", "山地剥", "水地比", "风地观", "雷地豫", "火地晋", "泽地萃", "天地否",
    "地山谦", "水山蹇", "风山渐", "雷山小过", "火山旅", "泽山咸", "天山遁", "地水师",
    "山水蒙", "坎为水", "风水涣", "雷水解", "火水未济", "泽水困", "天水讼", "地风升",
    "山风蛊", "巽为风", "雷风恒", "火风鼎", "泽风大过", "天风姤", "地雷复", "山雷颐",
    "水雷屯", "风雷益", "震为雷", "火雷噬嗑", "泽雷随", "天雷无妄", "地火明夷", "山火贲",
    "水火既济", "风火家人", "雷火丰", "离为火", "泽火革", "天火同人", "地泽临", "山泽损",
    "水泽节", "风泽中孚", "雷泽归妹", "火泽睽", "兑为泽", "天泽履", "地天泰", "山天大畜",
    "水天需", "风天小畜", "雷天大壮", "火天大有", "泽天夬", "乾为天", "泽天夬", "火天大有"
};

static void update_yao_display(void) {
    for (int i = 0; i < 6; i++) {
        int idx = 5 - i;  /* display top-to-bottom: 上(0) at top, 初(5) at bottom */
        int y = LINE_Y_START + i * LINE_GAP;
        int is_yang = s_yao[idx];

        /* Delete old line objects */
        if (s_lines[i][0]) { lv_obj_del(s_lines[i][0]); s_lines[i][0] = NULL; }
        if (s_lines[i][1]) { lv_obj_del(s_lines[i][1]); s_lines[i][1] = NULL; }

        if (is_yang) {
            /* Yang: single solid line */
            lv_obj_t *line = lv_obj_create(s_scr);
            lv_obj_set_size(line, LINE_W, LINE_H);
            lv_obj_set_pos(line, LINE_X, y);
            lv_obj_set_style_bg_color(line, COLOR_ORANGE, 0);
            lv_obj_set_style_radius(line, 4, 0);
            lv_obj_set_style_border_width(line, 0, 0);
            lv_obj_set_style_pad_all(line, 0, 0);
            s_lines[i][0] = line;
        } else {
            /* Yin: two short lines with gap */
            int seg_w = (LINE_W - 20) / 2;
            lv_obj_t *l = lv_obj_create(s_scr);
            lv_obj_set_size(l, seg_w, LINE_H);
            lv_obj_set_pos(l, LINE_X, y);
            lv_obj_set_style_bg_color(l, COLOR_ORANGE, 0);
            lv_obj_set_style_radius(l, 4, 0);
            lv_obj_set_style_border_width(l, 0, 0);
            lv_obj_set_style_pad_all(l, 0, 0);
            s_lines[i][0] = l;

            lv_obj_t *r = lv_obj_create(s_scr);
            lv_obj_set_size(r, seg_w, LINE_H);
            lv_obj_set_pos(r, LINE_X + seg_w + 20, y);
            lv_obj_set_style_bg_color(r, COLOR_ORANGE, 0);
            lv_obj_set_style_radius(r, 4, 0);
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            s_lines[i][1] = r;
        }
    }
}

static int get_hexagram_id(void) {
    /* lower trigram: 初(5), 二(4), 三(3) → bits 0,1,2 */
    /* upper trigram: 四(2), 五(1), 上(0) → bits 3,4,5 */
    int lower = s_yao[5] | (s_yao[4] << 1) | (s_yao[3] << 2);
    int upper = s_yao[2] | (s_yao[1] << 1) | (s_yao[0] << 2);
    return (upper << 3) | lower;
}

static void shake_cb(lv_event_t *e) {
    (void)e;
    /* Generate 6 random 爻 */
    for (int i = 0; i < 6; i++) {
        s_yao[i] = esp_random() & 1;
    }
    update_yao_display();

    /* Show hexagram name */
    int id = get_hexagram_id();
    if (id >= 0 && id < 64) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", hexagram_names[id]);
        lv_label_set_text(s_result_lbl, buf);
        lv_obj_clear_flag(s_result_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *fortune_screen_create(void) {
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

    /* Instruction */
    s_inst_lbl = lv_label_create(s_scr);
    lv_label_set_text(s_inst_lbl, "\xe7\x82\xb9\xe5\x87\xbb\xe5\xbc\x80\xe5\xa7\x8b"); /* 点击开始 */
    lv_obj_set_style_text_color(s_inst_lbl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(s_inst_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_inst_lbl, 10, 60);

    /* Initialize 爻 display */
    memset(s_lines, 0, sizeof(s_lines));
    update_yao_display();

    /* 爻 labels */
    for (int i = 0; i < 6; i++) {
        s_labels[i] = lv_label_create(s_scr);
        lv_label_set_text(s_labels[i], yao_names[i]);
        lv_obj_set_style_text_color(s_labels[i], COLOR_LABEL, 0);
        lv_obj_set_style_text_font(s_labels[i], &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(s_labels[i], LABEL_X, LINE_Y_START + i * LINE_GAP - 2);
    }

    /* Result label (hidden until shake) */
    s_result_lbl = lv_label_create(s_scr);
    lv_label_set_text(s_result_lbl, "");
    lv_obj_set_style_text_color(s_result_lbl, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_result_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_result_lbl, 10, 200);
    lv_obj_add_flag(s_result_lbl, LV_OBJ_FLAG_HIDDEN);

    /* Shake button */
    s_btn = lv_btn_create(s_scr);
    lv_obj_set_size(s_btn, BTN_W, BTN_H);
    lv_obj_set_pos(s_btn, LINE_X, BTN_Y);
    lv_obj_set_style_bg_color(s_btn, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(s_btn, 18, 0);
    lv_obj_add_event_cb(s_btn, shake_cb, LV_EVENT_CLICKED, NULL);

    s_btn_label = lv_label_create(s_btn);
    lv_label_set_text(s_btn_label, "\xe5\xbc\x80\xe5\xa7\x8b"); /* 开始 */
    lv_obj_set_style_text_font(s_btn_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(s_btn_label);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    return s_scr;
}
