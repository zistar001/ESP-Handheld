#include "fortune_screen.h"
#include "ui/components/status_bar.h"
#include "ui/components/theme_colors.h"
#include "ui/display_driver.h"
#include "modules/imu/imu_driver.h"
#include "modules/iching/iching_data.h"
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define COLOR_BG       CLR_BG
#define COLOR_ORANGE   CLR_ACCENT
#define COLOR_WHITE    CLR_TEXT
#define COLOR_GREY     CLR_SUBTEXT

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
static lv_obj_t *s_inst_label = NULL;       /* "心念专一，摇动起卦" — hidden in result phase */
static lv_obj_t *s_result_title = NULL;     /* "第X卦 XXX" — shown only in result phase */
static TaskHandle_t s_shake_task = NULL;
static bool s_shaking = false;
static int s_phase = 0;         /* 0=select, 1=shaking, 2=result */
static bool s_yao_ready = false;
static TickType_t s_yao_tick = 0;
static int shake_cycle = 0;

/* === Layout constants === */

/* Phase 0: category buttons — 6 items fit without scrolling */
#define CAT_BTN_H   30
#define CAT_STEP    36          /* 30 + 6px gap */
#define CAT_START_Y 54          /* below centered title (y=34, 16px height + 4px gap) */

/* Phase 1: shaking yao lines — evenly distributed from below text to bottom 10% */
#define YAO_W       180
#define YAO_H       12
#define YAO_GAP     23
#define YAO_BASE_Y  252         /* lowest line y, leaves ~28px (10%) bottom margin */
                                 /* top of highest line ≈ 252 - 5×35 = 77, just below text */

/* Phase 2: compact hexagram — ~25% of screen width, positioned up high */
#define HEX_W       60
#define HEX_H       4
#define HEX_GAP     3
#define HEX_BASE_Y  120         /* lowest line y, leaves space below for scrollable text */

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
    /* Fu Xi trigram numbering: TOP line = bit 0, BOTTOM line = bit 2.
     * s_yao[0]=bottom line, s_yao[2]=top of lower trigram, etc. */
    int lower = s_yao[2] | (s_yao[1] << 1) | (s_yao[0] << 2);
    int upper = s_yao[5] | (s_yao[4] << 1) | (s_yao[3] << 2);
    int binary = (upper << 3) | lower;
    /* g_iching[] is in King Wen order, not binary order */
    if (binary < 0 || binary >= 64) return 0;
    return binary_to_index[binary];
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
        "%s\n《象》曰：%s\n大象：%s\n%s：%s",
        h->gua_ci, h->xiang, h->daxiang,
        cat_names[s_category], advice ? advice : "");

    /* Remove old shake lines */
    for (int i = 0; i < 6; i++) {
        if (s_shake_lines[i][0]) { lv_obj_del(s_shake_lines[i][0]); s_shake_lines[i][0] = NULL; }
        if (s_shake_lines[i][1]) { lv_obj_del(s_shake_lines[i][1]); s_shake_lines[i][1] = NULL; }
    }

    /* Hide shake-phase instruction, show hexagram title in its place */
    if (s_inst_label) lv_obj_add_flag(s_inst_label, LV_OBJ_FLAG_HIDDEN);

    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "第%d卦 %s", id + 1, h->name);
    if (!s_result_title) {
        s_result_title = lv_label_create(s_scr);
        lv_obj_set_style_text_color(s_result_title, COLOR_ORANGE, 0);
        lv_obj_set_style_text_font(s_result_title, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_width(s_result_title, 240);
        lv_obj_set_style_text_align(s_result_title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_result_title, LV_ALIGN_TOP_MID, 0, 58);
    }
    lv_label_set_text(s_result_title, title_buf);

    /* Create scrollable result area below the compact hexagram */
    int hex_bottom = HEX_BASE_Y + HEX_H;
    int cont_y = hex_bottom + 8;
    int cont_h = 280 - cont_y - 8;

    lv_obj_t *cont = lv_obj_create(s_scr);
    lv_obj_set_size(cont, 240, cont_h);
    lv_obj_set_pos(cont, 0, cont_y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    /* Full judgment text — left-aligned */
    lv_obj_t *judgment = lv_label_create(cont);
    lv_label_set_text(judgment, buf);
    lv_obj_set_style_text_color(judgment, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(judgment, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(judgment, 220);
    lv_obj_set_pos(judgment, 10, 0);

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
                            /* 延迟0.5秒让用户看清最后一爻再显示结果 */
                            lvgl_unlock();
                            vTaskDelay(pdMS_TO_TICKS(500));
                            lvgl_lock();
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

    /* Centered instruction — hidden in result phase, replaced by s_result_title */
    s_inst_label = lv_label_create(s_scr);
    lv_label_set_text(s_inst_label, "心念专一，摇动起卦");
    lv_obj_set_style_text_color(s_inst_label, COLOR_GREY, 0);
    lv_obj_set_style_text_font(s_inst_label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(s_inst_label, 240);
    lv_obj_set_style_text_align(s_inst_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_inst_label, LV_ALIGN_TOP_MID, 0, 58);

    /* Initialize line pointers */
    for (int i = 0; i < 6; i++) {
        s_shake_lines[i][0] = s_shake_lines[i][1] = NULL;
        s_hex_lines[i][0]   = s_hex_lines[i][1]   = NULL;
    }
    s_judgment_label = NULL;
    s_result_title = NULL;

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
    /* Called from LVGL event (already inside lvgl_lock) — don't lock again */
    enter_shake_screen();
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
    /* Called from key_handler (already inside lvgl_lock) — don't lock again */
    enter_shake_screen();
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
