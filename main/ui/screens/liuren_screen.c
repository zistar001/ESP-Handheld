#include "liuren_screen.h"
#include "ui/components/status_bar.h"
#include "modules/iching/liuren_core.h"
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "freertos/FreeRTOS.h"

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_WHITE    lv_color_hex(0xFFFFFF)
#define COLOR_GREY     lv_color_hex(0x999999)
#define COLOR_ORANGE   lv_color_hex(0xFF5C00)
#define COLOR_YELLOW   lv_color_hex(0xFFB800)
#define COLOR_DIM      lv_color_hex(0x555555)

/* 手掌指节方块 */
static const int s_seg_x[] = {29,29, 66,66,66, 106,106,106, 145,145,145, 180,180,180, 29};
static const int s_seg_y[] = {78,126, 56,99,141, 44,90,136, 56,99,141, 75,106,141, 168};
static const int s_seg_w[] = {29,29, 25,25,25, 25,25,25, 25,25,25, 20,20,20, 171};
static const int s_seg_h[] = {43,43, 37,37,31, 41,41,36, 37,37,31, 26,30,27, 14};
/* 6神->指节: 大安=4, 留连=2, 速喜=5, 赤口=8, 小吉=10, 空亡=7 */
static const int s_god2seg[] = {4,2,5,8,10,7};
static const int s_cycle[] = {4,2,5,8,10,7};
/* 六神名(竖直排列, 直接写带\n的字符串避免UTF-8字节处理) */
static const char *s_ltxt[] = {"大\n安","留\n连","速\n喜","赤\n口","小\n吉","空\n亡"};
/* 六神名(原始不带\n, 用于结果显示) */
static const char *s_ltxt_flat[] = {"大安","留连","速喜","赤口","小吉","空亡"};
/* 计算方式: 指节中心 (x+w/2, y+h/2), 文字左上 (cx-w/2, cy-h/2),
   2字竖排约16x32, 3字竖排约16x48 */
static const int s_lx[] = {71, 70, 111, 150, 150, 111}; /* 速喜赤口小吉空亡左移3px */
static const int s_ly[] = {139, 55, 44, 56, 139, 132}; /* 全部上移8px(除食指中指) */
/* 手指名(竖直, 精简) */
static const char *s_finger[] = {"大\n拇","食\n指","中\n指","无\n名","小"};
static const int s_fx[] = {37,72,112,151,182}; /* 小左移3px */
static const int s_fy[] = {77,100,91,100,106}; /* 大拇上移8px */

static lv_obj_t *s_segs[15];
static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_desc[3];
static lv_obj_t *s_btn;
static lv_obj_t *s_scroll_cont = NULL;
static lv_obj_t *s_spirit_labels[6] = {NULL};
static lv_obj_t *s_finger_labels[5] = {NULL};

static void reset_segs(void) {
    for (int i = 0; i < 15; i++)
        if (s_segs[i]) lv_obj_set_style_bg_color(s_segs[i], COLOR_WHITE, 0);
}

static void light_seg(int idx) {
    for (int i = 0; i < 15; i++)
        if (s_segs[i]) lv_obj_set_style_bg_color(s_segs[i], (i == idx) ? COLOR_YELLOW : COLOR_WHITE, 0);
}

static void calc_cb(lv_event_t *e) { (void)e; liuren_screen_calculate(); }

void liuren_screen_calculate(void) {
    lunar_date_t ld = {2024, 1, 1, 0};
    int hour = 0;
    bool time_ok = (liuren_get_lunar_today(&ld) == 0);
    int h = liuren_get_current_hour();
    if (h >= 0) hour = h;

    liuren_result_t res = liuren_calculate(ld.month, ld.day, hour);
    int result_pos = (res.position >= 0 && res.position < 6) ? res.position : 0;
    int result_seg = s_god2seg[result_pos];

    /* === 动画：依次点亮 === */
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 6; i++) {
            light_seg(s_cycle[i]);
            lv_refr_now(NULL);
            vTaskDelay(pdMS_TO_TICKS(60));
        }
    }
    for (int i = 0; i < 6; i++) {
        light_seg(s_cycle[i]);
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(80));
        if (s_cycle[i] == result_seg) break;
    }
    reset_segs();
    light_seg(result_seg);

    /* 延迟0.5秒让用户看清结果位置再切换布局 */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* === 动画完成：隐藏手掌，显示紧凑结果 === */
    for (int i = 0; i < 15; i++)
        if (s_segs[i]) lv_obj_add_flag(s_segs[i], LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 6; i++)
        if (s_spirit_labels[i]) lv_obj_add_flag(s_spirit_labels[i], LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 5; i++)
        if (s_finger_labels[i]) lv_obj_add_flag(s_finger_labels[i], LV_OBJ_FLAG_HIDDEN);

    /* 只显示结果六神名 — 居中 */
    lv_obj_t *result_label = lv_label_create(s_scr);
    lv_label_set_text(result_label, s_ltxt_flat[result_pos]);
    lv_obj_set_style_text_color(result_label, COLOR_YELLOW, 0);
    lv_obj_set_style_text_font(result_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(result_label, 240);
    lv_obj_set_style_text_align(result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(result_label, LV_ALIGN_TOP_MID, 0, 56);
    /* 下方结果横线 */
    lv_obj_t *under = lv_obj_create(s_scr);
    lv_obj_set_size(under, 40, 3);
    lv_obj_align(under, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_color(under, COLOR_YELLOW, 0);
    lv_obj_set_style_border_width(under, 0, 0);
    lv_obj_set_style_radius(under, 1, 0);
    lv_obj_set_style_pad_all(under, 0, 0);

    /* 隐藏解释文字和按钮 */
    for (int i = 0; i < 3; i++)
        if (s_desc[i]) lv_obj_add_flag(s_desc[i], LV_OBJ_FLAG_HIDDEN);
    if (s_btn) lv_obj_add_flag(s_btn, LV_OBJ_FLAG_HIDDEN);

    /* 滚动结果容器 */
    int cont_y = 85;
    int cont_h = 280 - cont_y - 8;

    if (s_scroll_cont) lv_obj_del(s_scroll_cont);
    s_scroll_cont = lv_obj_create(s_scr);
    lv_obj_set_size(s_scroll_cont, 240, cont_h);
    lv_obj_set_pos(s_scroll_cont, 0, cont_y);
    lv_obj_set_style_bg_opa(s_scroll_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_scroll_cont, 0, 0);
    lv_obj_set_style_radius(s_scroll_cont, 0, 0);
    lv_obj_set_style_pad_all(s_scroll_cont, 0, 0);
    lv_obj_set_scroll_dir(s_scroll_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_scroll_cont, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *label = lv_label_create(s_scroll_cont);
    char buf[600];
    if (time_ok) {
        snprintf(buf, sizeof(buf),
            "农历 %d月%d日 %s\n【%s】%s\n%s",
            ld.month, ld.day, liuren_hour_name(hour),
            res.name, res.meaning, res.judgment);
    } else {
        snprintf(buf, sizeof(buf), "【%s】%s\n%s", res.name, res.meaning, res.judgment);
    }
    lv_label_set_text(label, buf);
    lv_obj_set_style_text_color(label, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(label, 220);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 10, 0);
}

lv_obj_t *liuren_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COLOR_BG, 0);
    status_bar_create(s_scr);

    lv_obj_t *t = lv_label_create(s_scr);
    lv_label_set_text(t, "小六壬");
    lv_obj_set_style_text_color(t, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(t, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(t, 10, 34);

    /* 重置所有静态指针(旧屏幕已删除, 指针已失效) */
    s_scroll_cont = NULL;
    for (int i = 0; i < 6; i++) s_spirit_labels[i] = NULL;
    for (int i = 0; i < 5; i++) s_finger_labels[i] = NULL;

    /* 手掌指节方块 */
    for (int i = 0; i < 15; i++) {
        s_segs[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_segs[i], s_seg_w[i], s_seg_h[i]);
        lv_obj_set_pos(s_segs[i], s_seg_x[i], s_seg_y[i]);
        lv_obj_set_style_bg_color(s_segs[i], COLOR_WHITE, 0);
        lv_obj_set_style_radius(s_segs[i], 5, 0);
        lv_obj_set_style_border_width(s_segs[i], 0, 0);
        lv_obj_set_style_pad_all(s_segs[i], 0, 0);
    }

    /* 六神名 — 在指节内居中竖直排列 */
    for (int i = 0; i < 6; i++) {
        lv_obj_t *l = lv_label_create(s_scr);
        lv_label_set_text(l, s_ltxt[i]);
        lv_obj_set_style_text_color(l, COLOR_ORANGE, 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(l, s_lx[i], s_ly[i]);
        s_spirit_labels[i] = l;
    }

    /* 手指名(竖直, 精简) */
    for (int i = 0; i < 5; i++) {
        lv_obj_t *fl = lv_label_create(s_scr);
        lv_label_set_text(fl, s_finger[i]);
        lv_obj_set_pos(fl, s_fx[i], s_fy[i]);
        lv_obj_set_style_text_color(fl, COLOR_GREY, 0);
        lv_obj_set_style_text_font(fl, &lv_font_simsun_16_cjk, 0);
        s_finger_labels[i] = fl;
    }

    /* 六神解释文字 — 居中 */
    s_desc[0] = lv_label_create(s_scr);
    lv_label_set_text(s_desc[0], "大安:大吉     留连:拖延");
    lv_obj_set_style_text_color(s_desc[0], COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc[0], &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(s_desc[0], 220);
    lv_obj_set_style_text_align(s_desc[0], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_desc[0], 10, 184);

    s_desc[1] = lv_label_create(s_scr);
    lv_label_set_text(s_desc[1], "速喜:喜事     赤口:争执");
    lv_obj_set_style_text_color(s_desc[1], COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc[1], &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(s_desc[1], 220);
    lv_obj_set_style_text_align(s_desc[1], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_desc[1], 10, 199);

    s_desc[2] = lv_label_create(s_scr);
    lv_label_set_text(s_desc[2], "小吉:好结果   空亡:大不利");
    lv_obj_set_style_text_color(s_desc[2], COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc[2], &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(s_desc[2], 220);
    lv_obj_set_style_text_align(s_desc[2], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_desc[2], 10, 214);

    s_btn = lv_btn_create(s_scr);
    lv_obj_set_size(s_btn, 125, 23);
    lv_obj_set_pos(s_btn, 56, 237);
    lv_obj_set_style_bg_color(s_btn, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(s_btn, 18, 0);
    lv_obj_add_event_cb(s_btn, calc_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *b = lv_label_create(s_btn);
    lv_label_set_text(b, "掐指一算");
    lv_obj_set_style_text_color(b, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(b, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(b);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    return s_scr;
}

void liuren_screen_navigate(int dir) {
    if (s_scroll_cont) lv_obj_scroll_by(s_scroll_cont, 0, -dir * 20, LV_ANIM_ON);
}
