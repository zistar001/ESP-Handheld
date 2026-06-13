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
#define COLOR_SEG      lv_color_hex(0xFFFFFF)
#define COLOR_HL       lv_color_hex(0xFFB800)

static const int s_seg_x[] = {29,29, 66,66,66, 106,106,106, 145,145,145, 180,180,180, 29};
static const int s_seg_y[] = {92,140, 70,113,155, 58,104,150, 70,113,155, 89,120,155, 182};
static const int s_seg_w[] = {29,29, 25,25,25, 25,25,25, 25,25,25, 20,20,20, 171};
static const int s_seg_h[] = {43,43, 37,37,31, 41,41,36, 37,37,31, 26,30,27, 14};
/* 6神->指节: 大安=4(食指根部), 留连=2(食指尖端), 速喜=5(中指尖端), 赤口=8(无名指尖端), 小吉=10(无名指根部), 空亡=7(中指根部) */
static const int s_god2seg[] = {4,2,5,8,10,7};
static const int s_cycle[] = {4,2,5,8,10,7};
static const int s_lx[] = {74,72,114,153,153,114};
static const int s_ly[] = {155,73,62,73,155,151};
static const char *s_ltxt[] = {"大安","留连","速喜","赤口","小吉","空亡"};

static lv_obj_t *s_segs[15];
static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_desc[3];
static lv_obj_t *s_btn;
static lv_obj_t *s_scroll_cont = NULL;

static void reset_segs(void) {
    for (int i = 0; i < 15; i++)
        if (s_segs[i]) lv_obj_set_style_bg_color(s_segs[i], COLOR_SEG, 0);
}

static void light_seg(int idx) {
    for (int i = 0; i < 15; i++)
        if (s_segs[i]) lv_obj_set_style_bg_color(s_segs[i], (i == idx) ? COLOR_HL : COLOR_SEG, 0);
}

static void calc_cb(lv_event_t *e) { (void)e; liuren_screen_calculate(); }

void liuren_screen_calculate(void) {
    lunar_date_t ld = {2024, 1, 1, 0};
    int hour = 0;
    bool time_ok = (liuren_get_lunar_today(&ld) == 0);
    int h = liuren_get_current_hour();
    if (h >= 0) hour = h;

    liuren_result_t res = liuren_calculate(ld.month, ld.day, hour);
    int result_seg = (res.position >= 0 && res.position < 6) ? s_god2seg[res.position] : 0;

    /* 依次点亮: 大安→留连→速喜→赤口→小吉→空亡, 循环3轮 */
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 6; i++) {
            light_seg(s_cycle[i]);
            lv_refr_now(NULL);
            vTaskDelay(pdMS_TO_TICKS(60));
        }
    }
    /* 最终停在结果位置 */
    for (int i = 0; i < 6; i++) {
        light_seg(s_cycle[i]);
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(80));
        if (s_cycle[i] == result_seg) break;
    }
    /* 最终结果高亮 */
    reset_segs();
    light_seg(result_seg);

    /* 隐藏6神解释和按钮 */
    for (int i = 0; i < 3; i++)
        if (s_desc[i]) lv_obj_add_flag(s_desc[i], LV_OBJ_FLAG_HIDDEN);
    if (s_btn) lv_obj_add_flag(s_btn, LV_OBJ_FLAG_HIDDEN);

    /* 创建可滚动结果容器 */
    if (s_scroll_cont) lv_obj_del(s_scroll_cont);
    s_scroll_cont = lv_obj_create(s_scr);
    lv_obj_set_size(s_scroll_cont, 240, 80);
    lv_obj_set_pos(s_scroll_cont, 0, 196);
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

    for (int i = 0; i < 15; i++) {
        s_segs[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_segs[i], s_seg_w[i], s_seg_h[i]);
        lv_obj_set_pos(s_segs[i], s_seg_x[i], s_seg_y[i]);
        lv_obj_set_style_bg_color(s_segs[i], COLOR_SEG, 0);
        lv_obj_set_style_radius(s_segs[i], 5, 0);
        lv_obj_set_style_border_width(s_segs[i], 0, 0);
        lv_obj_set_style_pad_all(s_segs[i], 0, 0);
    }

    for (int i = 0; i < 6; i++) {
        lv_obj_t *l = lv_label_create(s_scr);
        lv_label_set_text(l, s_ltxt[i]);
        lv_obj_set_style_text_color(l, COLOR_ORANGE, 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(l, s_lx[i], s_ly[i]);
    }

    lv_obj_t *fl;
    fl = lv_label_create(s_scr); lv_label_set_text(fl, "大拇指"); lv_obj_set_pos(fl, 33, 98);
    lv_obj_set_style_text_color(fl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(fl, &lv_font_simsun_16_cjk, 0);
    fl = lv_label_create(s_scr); lv_label_set_text(fl, "食指"); lv_obj_set_pos(fl, 70, 113);
    lv_obj_set_style_text_color(fl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(fl, &lv_font_simsun_16_cjk, 0);
    fl = lv_label_create(s_scr); lv_label_set_text(fl, "中指"); lv_obj_set_pos(fl, 110, 104);
    lv_obj_set_style_text_color(fl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(fl, &lv_font_simsun_16_cjk, 0);
    fl = lv_label_create(s_scr); lv_label_set_text(fl, "无名指"); lv_obj_set_pos(fl, 149, 112);
    lv_obj_set_style_text_color(fl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(fl, &lv_font_simsun_16_cjk, 0);
    fl = lv_label_create(s_scr); lv_label_set_text(fl, "小拇指"); lv_obj_set_pos(fl, 183, 120);
    lv_obj_set_style_text_color(fl, COLOR_GREY, 0);
    lv_obj_set_style_text_font(fl, &lv_font_simsun_16_cjk, 0);

    s_desc[0] = lv_label_create(s_scr);
    lv_label_set_text(s_desc[0], "大安:大吉     留连:拖延");
    lv_obj_set_style_text_color(s_desc[0], COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc[0], &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_desc[0], 61, 196);
    s_desc[1] = lv_label_create(s_scr);
    lv_label_set_text(s_desc[1], "速喜:喜事     赤口:争执");
    lv_obj_set_style_text_color(s_desc[1], COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc[1], &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_desc[1], 61, 208);
    s_desc[2] = lv_label_create(s_scr);
    lv_label_set_text(s_desc[2], "小吉:好结果   空亡:大不利");
    lv_obj_set_style_text_color(s_desc[2], COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_desc[2], &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_desc[2], 61, 220);

    s_btn = lv_btn_create(s_scr);
    lv_obj_set_size(s_btn, 125, 23);
    lv_obj_set_pos(s_btn, 56, 243);
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
