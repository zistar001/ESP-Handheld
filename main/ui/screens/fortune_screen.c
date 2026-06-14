#include "fortune_screen.h"
#include "ui/components/status_bar.h"
#include "ui/display_driver.h"
#include "modules/imu/imu_driver.h"
#include "modules/iching/iching_data.h"
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_ORANGE   lv_color_hex(0xFF5C00)
#define COLOR_WHITE    lv_color_hex(0xFFFFFF)
#define COLOR_GREY     lv_color_hex(0x999999)

static const char *categories[] = {"运势", "事业", "经商", "求名", "婚恋", "决策"};
static int s_category = 0;
static int s_sel = 0;
static int s_yao[6] = {0};
static int s_yao_count = 0;

static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_shake_lines[6][2];   /* yao lines during shaking (phase 1) */
static lv_obj_t *s_hex_lines[6][2];     /* compact hexagram (phase 2) */
static lv_obj_t *s_cat_btns[6];
static lv_obj_t *s_judgment_label = NULL;
static TaskHandle_t s_shake_task = NULL;
static bool s_shaking = false;
static int s_phase = 0;         /* 0=select, 1=shaking, 2=result */
static bool s_yao_ready = false;
static TickType_t s_yao_tick = 0;
static int shake_cycle = 0;

/* === Layout constants === */

/* Phase 0: category buttons — 6 items fit without scrolling */
#define CAT_BTN_H   32
#define CAT_STEP    38          /* 32 + 6px gap */
#define CAT_START_Y 48

/* Phase 1: shaking yao lines — grow from bottom up */
#define YAO_W       180
#define YAO_H       8
#define YAO_GAP     5
#define YAO_BASE_Y  252         /* lowest line y, leave ~28px (10%) bottom margin */

/* Phase 2: compact hexagram — ~25% of screen width */
#define HEX_W       60
#define HEX_H       5
#define HEX_GAP     3
#define HEX_BASE_Y  248         /* lowest line y of compact hexagram */

/* Shake detection */
static const float SHAKE_THRESHOLD = 0.6f;
static const float REST_THRESHOLD  = 0.2f;

/* ================================================================
 *  UI helpers
 * ================================================================ */

/* Create a single yao line (rectangle) at (x, y) with given dimensions */
static lv_obj_t *yao_line_create(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, COLOR_ORANGE, 0);
    lv_obj_set_style_radius(obj, 3, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

/* Delete all stored line objects (both shake and hex) */
static void clear_all_lines(void) {
    for (int i = 0; i < 6; i++) {
        if (s_shake_lines[i][0]) { lv_obj_del(s_shake_lines[i][0]); s_shake_lines[i][0] = NULL; }
        if (s_shake_lines[i][1]) { lv_obj_del(s_shake_lines[i][1]); s_shake_lines[i][1] = NULL; }
        if (s_hex_lines[i][0])   { lv_obj_del(s_hex_lines[i][0]);   s_hex_lines[i][0] = NULL; }
        if (s_hex_lines[i][1])   { lv_obj_del(s_hex_lines[i][1]);   s_hex_lines[i][1] = NULL; }
    }
}

/* ================================================================
 *  Yao line drawing — phase 1 (shaking, bottom-up)
 * ================================================================ */
static void draw_shake_yao(void) {
    /* Remove old shake lines */
    for (int i = 0; i < 6; i++) {
        if (s_shake_lines[i][0]) { lv_obj_del(s_shake_lines[i][0]); s_shake_lines[i][0] = NULL; }
        if (s_shake_lines[i][1]) { lv_obj_del(s_shake_lines[i][1]); s_shake_lines[i][1] = NULL; }
    }
    /* Draw from bottom (i=0 = first yao generated) to top */
    int half = YAO_W * 0.45;
    for (int i = 0; i < s_yao_count && i < 6; i++) {
        int y = YAO_BASE_Y - i * (YAO_H + YAO_GAP);
        int cx = (240 - YAO_W) / 2;
        if (s_yao[i]) {
            /* Yang: solid line */
            s_shake_lines[i][0] = yao_line_create(s_scr, cx, y, YAO_W, YAO_H);
        } else {
            /* Yin: two halves */
            s_shake_lines[i][0] = yao_line_create(s_scr, cx, y, half, YAO_H);
            s_shake_lines[i][1] = yao_line_create(s_scr, cx + YAO_W - half, y, half, YAO_H);
        }
    }
}

/* ================================================================
 *  Compact hexagram drawing — phase 2 (result)
 * ================================================================ */
static void draw_compact_hexagram(void) {
    int half = HEX_W * 0.45;
    int cx = (240 - HEX_W) / 2;
    for (int i = 0; i < 6; i++) {
        int y = HEX_BASE_Y - i * (HEX_H + HEX_GAP);
        if (s_yao[i]) {
            s_hex_lines[i][0] = yao_line_create(s_scr, cx, y, HEX_W, HEX_H);
        } else {
            s_hex_lines[i][0] = yao_line_create(s_scr, cx, y, half, HEX_H);
            s_hex_lines[i][1] = yao_line_create(s_scr, cx + HEX_W - half, y, half, HEX_H);
        }
    }
}

/* ================================================================
 *  I Ching logic — get hexagram ID, show result
 * ================================================================ */
static int get_hexagram_id(void) {
    int lower = s_yao[0] | (s_yao[1] << 1) | (s_yao[2] << 2);
    int upper = s_yao[3] | (s_yao[4] << 1) | (s_yao[5] << 2);
    return (upper << 3) | lower;
}

static void show_result(void) {
    int id = get_hexagram_id();
    if (id < 0 || id >= 64) return;
    s_phase = 2;
    const iching_hexagram_t *h = &g_iching[id];
    const char *advice = "";
    switch (s_category) {
        case 0: advice = h->yushi; break;
        case 1: advice = h->shiye; break;
        case 2: advice = h->jingshang; break;
        case 3: advice = h->qiuming; break;
        case 4: advice = h->hunlian; break;
        case 5: advice = h->juece; break;
    }
    static char buf[600];
    const char *cat_names[] = {"运势", "事业", "经商", "求名", "婚恋", "决策"};
    snprintf(buf, sizeof(buf),
        "第%d卦 %s\n%s\n《象》曰：%s\n大象：%s\n%s：%s",
        id + 1, h->name, h->gua_ci, h->xiang, h->daxiang,
        cat_names[s_category], advice ? advice : "");

    /* Remove old shake lines, show compact hexagram instead */
    for (int i = 0; i < 6; i++) {
        if (s_shake_lines[i][0]) { lv_obj_del(s_shake_lines[i][0]); s_shake_lines[i][0] = NULL; }
        if (s_shake_lines[i][1]) { lv_obj_del(s_shake_lines[i][1]); s_shake_lines[i][1] = NULL; }
    }

    /* Create scrollable result area below the compact hexagram */
    int hex_bottom = HEX_BASE_Y + HEX_H;  /* bottom of compact hexagram area */
    int cont_y = hex_bottom + 8;
    int cont_h = 280 - cont_y - 8;        /* leave 8px bottom margin */

    lv_obj_t *cont = lv_obj_create(s_scr);
    lv_obj_set_size(cont, 240, cont_h);
    lv_obj_set_pos(cont, 0, cont_y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    /* Hexagram name + number centered */
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "第%d卦 %s  %s", id + 1, h->name, h->daxiang);
    lv_obj_t *hex_title = lv_label_create(cont);
    lv_label_set_text(hex_title, title_buf);
    lv_obj_set_style_text_color(hex_title, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(hex_title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(hex_title, 220);
    lv_obj_align(hex_title, LV_ALIGN_TOP_MID, 0, 0);

    /* Full judgment text below */
    lv_obj_t *judgment = lv_label_create(cont);
    lv_label_set_text(judgment, buf);
    lv_obj_set_style_text_color(judgment, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(judgment, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(judgment, 220);
    lv_obj_align(judgment, LV_ALIGN_TOP_MID, 0, 30);

    s_judgment_label = judgment;
}

/* ================================================================
 *  Shake detection task
 * ================================================================ */
static void shake_task(void *arg) {
    (void)arg;
    TickType_t last = 0;
    imu_data_t imu;
    int above = 0;
    shake_cycle = 0;
    s_yao_ready = true;

    while (s_shaking) {
        if (imu_read(&imu) == ESP_OK) {
            float ay_mag = imu.ay * imu.ay;
            float thresh = SHAKE_THRESHOLD * SHAKE_THRESHOLD;
            float rest   = REST_THRESHOLD  * REST_THRESHOLD;

            TickType_t now = xTaskGetTickCount();
            if (now - s_yao_tick < pdMS_TO_TICKS(600)) {
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }

            if (ay_mag > thresh && !above) {
                above = 1;
            } else if (ay_mag < rest && above) {
                above = 0;
                shake_cycle++;
                if (shake_cycle >= 3) {
                    shake_cycle = 0;
                    if (s_yao_count < 6) {
                        s_yao[s_yao_count] = esp_random() & 1;
                        s_yao_count++;
                        s_yao_tick = xTaskGetTickCount();
                        lvgl_lock();
                        clear_all_lines();
                        draw_shake_yao();
                        if (s_yao_count >= 6) {
                            draw_compact_hexagram();
                            show_result();
                            s_shaking = false;
                        }
                        lvgl_unlock();
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelete(NULL);
}

/* ================================================================
 *  Create the shaking/result screen (phase 1+2)
 * ================================================================ */
static void enter_shake_screen(void) {
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COLOR_BG, 0);
    status_bar_create(s_scr);

    /* Centered title */
    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, categories[s_category]);
    lv_obj_set_style_text_color(title, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    /* Centered instruction */
    lv_obj_t *inst = lv_label_create(s_scr);
    lv_label_set_text(inst, "心念专一，摇动起卦");
    lv_obj_set_style_text_color(inst, COLOR_GREY, 0);
    lv_obj_set_style_text_font(inst, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(inst, 240);
    lv_obj_set_style_text_align(inst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(inst, LV_ALIGN_TOP_MID, 0, 58);

    /* Initialize line pointers */
    for (int i = 0; i < 6; i++) {
        s_shake_lines[i][0] = s_shake_lines[i][1] = NULL;
        s_hex_lines[i][0]   = s_hex_lines[i][1]   = NULL;
    }
    s_judgment_label = NULL;

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);

    s_phase = 1;
    s_yao_count = 0;
    shake_cycle = 0;
    s_shaking = true;
    xTaskCreatePinnedToCore(shake_task, "shake_det", 2048, NULL, 2, &s_shake_task, 1);
}

/* ================================================================
 *  Category button click handler
 * ================================================================ */
static void cat_select_cb(lv_event_t *e) {
    s_category = (int)(intptr_t)lv_event_get_user_data(e);
    lvgl_lock();
    enter_shake_screen();
    lvgl_unlock();
}

/* ================================================================
 *  Navigation & selection (called from main.c key_handler)
 * ================================================================ */
void fortune_screen_navigate(int dir) {
    if (s_phase == 0) {
        int old = s_sel;
        s_sel += dir;
        if (s_sel < 0) s_sel = 0;
        if (s_sel > 5) s_sel = 5;
        if (s_sel != old) {
            for (int i = 0; i < 6; i++) {
                if (s_cat_btns[i]) {
                    lv_obj_set_style_bg_color(s_cat_btns[i],
                        i == s_sel ? COLOR_ORANGE : lv_color_hex(0x333333), 0);
                    lv_obj_set_style_border_width(s_cat_btns[i],
                        i == s_sel ? 2 : 0, 0);
                }
            }
        }
    } else if (s_phase == 2 && s_judgment_label) {
        /* Scroll the parent container of the judgment label */
        lv_obj_t *parent = lv_obj_get_parent(s_judgment_label);
        if (parent) lv_obj_scroll_by(parent, 0, -dir * 20, LV_ANIM_ON);
    }
}

void fortune_screen_select(void) {
    if (s_phase != 0) return;
    if (s_sel < 0 || s_sel > 5) return;
    s_category = s_sel;
    lvgl_lock();
    enter_shake_screen();
    lvgl_unlock();
}

/* ================================================================
 *  Phase 0: Category selection screen
 * ================================================================ */
lv_obj_t *fortune_screen_create(void) {
    s_category = 0;
    s_yao_count = 0;
    s_sel = 0;
    s_phase = 0;
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    status_bar_create(scr);

    /* Centered title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "选择占卜事项");
    lv_obj_set_style_text_color(title, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    /* 6 category buttons — all visible without scrolling */
    for (int i = 0; i < 6; i++) {
        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 200, CAT_BTN_H);
        lv_obj_set_pos(btn, 20, CAT_START_Y + i * CAT_STEP);
        lv_obj_set_style_bg_color(btn, i == s_sel ? COLOR_ORANGE : lv_color_hex(0x333333), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_width(btn, i == s_sel ? 2 : 0, 0);
        lv_obj_set_style_border_color(btn, COLOR_WHITE, 0);
        lv_obj_add_event_cb(btn, cat_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, categories[i]);
        lv_obj_set_style_text_color(l, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(l);
        s_cat_btns[i] = btn;
    }

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}
