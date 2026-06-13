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
static lv_obj_t *s_result;
static lv_obj_t *s_lines[6][2];
static lv_obj_t *s_cat_btns[6];
static lv_obj_t *s_scroll_cont = NULL;
static TaskHandle_t s_shake_task = NULL;
static bool s_shaking = false;
static int s_phase = 0; /* 0=category, 1=shaking, 2=result */
static bool s_yao_ready = false; /* true if a new yao was just generated (cooldown) */
static TickType_t s_yao_tick = 0; /* last yao generation time */

/* Shake: 3+ strong shakes = 1 yao */
#define SHAKE_THRESHOLD 0.5f
static int shake_cycle = 0;

static void update_yao_display(void) {
    for (int i = 0; i < 6; i++) {
        if (s_lines[i][0]) { lv_obj_del(s_lines[i][0]); s_lines[i][0] = NULL; }
        if (s_lines[i][1]) { lv_obj_del(s_lines[i][1]); s_lines[i][1] = NULL; }
    }
    for (int i = 0; i < s_yao_count; i++) {
        int y = 75 + (5 - i) * 18;
        if (s_yao[i]) {
            lv_obj_t *l = lv_obj_create(s_scr);
            lv_obj_set_size(l, 180, 10); lv_obj_set_pos(l, 30, y);
            lv_obj_set_style_bg_color(l, COLOR_ORANGE, 0);
            lv_obj_set_style_radius(l, 4, 0);
            lv_obj_set_style_border_width(l, 0, 0);
            lv_obj_set_style_pad_all(l, 0, 0);
            s_lines[5-i][0] = l;
        } else {
            lv_obj_t *l = lv_obj_create(s_scr);
            lv_obj_set_size(l, 80, 10); lv_obj_set_pos(l, 30, y);
            lv_obj_set_style_bg_color(l, COLOR_ORANGE, 0);
            lv_obj_set_style_radius(l, 4, 0);
            lv_obj_set_style_border_width(l, 0, 0);
            lv_obj_set_style_pad_all(l, 0, 0);
            s_lines[5-i][0] = l;
            lv_obj_t *r = lv_obj_create(s_scr);
            lv_obj_set_size(r, 80, 10); lv_obj_set_pos(r, 130, y);
            lv_obj_set_style_bg_color(r, COLOR_ORANGE, 0);
            lv_obj_set_style_radius(r, 4, 0);
            lv_obj_set_style_border_width(r, 0, 0);
            lv_obj_set_style_pad_all(r, 0, 0);
            s_lines[5-i][1] = r;
        }
    }
}

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

    /* Create scrollable container for result text, below 爻 lines */
    s_scroll_cont = lv_obj_create(s_scr);
    lv_obj_set_size(s_scroll_cont, 240, 70);
    lv_obj_set_pos(s_scroll_cont, 0, 170);
    lv_obj_set_style_bg_opa(s_scroll_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_scroll_cont, 0, 0);
    lv_obj_set_style_radius(s_scroll_cont, 0, 0);
    lv_obj_set_style_pad_all(s_scroll_cont, 0, 0);
    lv_obj_set_scroll_dir(s_scroll_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_scroll_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(s_scroll_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *label = lv_label_create(s_scroll_cont);
    lv_label_set_text(label, buf);
    lv_obj_set_style_text_color(label, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_width(label, 220);

    /* Hide the old static result label */
    lv_obj_add_flag(s_result, LV_OBJ_FLAG_HIDDEN);
}

static void shake_task(void *arg) {
    (void)arg;
    TickType_t last = 0;
    imu_data_t imu;
    int above = 0;
    shake_cycle = 0;
    s_yao_ready = true;

    while (s_shaking) {
        if (imu_read(&imu) == ESP_OK) {
            /* Detect up/down shaking using Y-axis acceleration */
            float ay_mag = imu.ay * imu.ay;
            float thresh = 0.6f * 0.6f;  /* 0.6g deviation from 1g */
            float rest = 0.2f * 0.2f;

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
                        update_yao_display();
                        if (s_yao_count >= 6) {
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

static void cat_select_cb(lv_event_t *e) {
    s_category = (int)(intptr_t)lv_event_get_user_data(e);
    lvgl_lock();
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COLOR_BG, 0);
    status_bar_create(s_scr);

    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, categories[s_category]);
    lv_obj_set_style_text_color(title, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 10, 34);

    lv_obj_t *inst = lv_label_create(s_scr);
    lv_label_set_text(inst, "心念专一，摇动起卦");
    lv_obj_set_style_text_color(inst, COLOR_GREY, 0);
    lv_obj_set_style_text_font(inst, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(inst, 10, 60);

    for (int i = 0; i < 6; i++) s_lines[i][0] = s_lines[i][1] = NULL;

    s_result = lv_label_create(s_scr);
    lv_label_set_text(s_result, "");
    lv_obj_set_style_text_color(s_result, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_result, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_result, 10, 200);
    lv_obj_set_width(s_result, 220);
    lv_obj_add_flag(s_result, LV_OBJ_FLAG_HIDDEN);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    lvgl_unlock();

    s_phase = 1;
    s_yao_count = 0;
    shake_cycle = 0;
    s_shaking = true;
    xTaskCreatePinnedToCore(shake_task, "shake_det", 2048, NULL, 2, &s_shake_task, 1);
}

void fortune_screen_navigate(int dir) {
    if (s_phase == 0) {
        /* Category selection: navigate items */
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
    } else if (s_phase == 2 && s_scroll_cont) {
        /* Result screen: scroll content container */
        lv_obj_scroll_by(s_scroll_cont, 0, -dir * 20, LV_ANIM_ON);
    }
}

void fortune_screen_select(void) {
    if (s_phase != 0) return;
    if (s_sel < 0 || s_sel > 5) return;
    s_category = s_sel;
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COLOR_BG, 0);
    status_bar_create(s_scr);

    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, categories[s_category]);
    lv_obj_set_style_text_color(title, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 10, 34);

    lv_obj_t *inst = lv_label_create(s_scr);
    lv_label_set_text(inst, "心念专一，摇动起卦");
    lv_obj_set_style_text_color(inst, COLOR_GREY, 0);
    lv_obj_set_style_text_font(inst, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(inst, 10, 60);

    for (int i = 0; i < 6; i++) s_lines[i][0] = s_lines[i][1] = NULL;

    s_result = lv_label_create(s_scr);
    lv_label_set_text(s_result, "");
    lv_obj_set_style_text_color(s_result, COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(s_result, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(s_result, 10, 200);
    lv_obj_set_width(s_result, 220);
    lv_obj_add_flag(s_result, LV_OBJ_FLAG_HIDDEN);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);

    s_phase = 1;
    s_yao_count = 0;
    shake_cycle = 0;
    s_shaking = true;
    xTaskCreatePinnedToCore(shake_task, "shake_det", 2048, NULL, 2, &s_shake_task, 1);
}

lv_obj_t *fortune_screen_create(void) {
    s_category = 0;
    s_yao_count = 0;
    s_sel = 0;
    s_phase = 0;
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "选择占卜事项");
    lv_obj_set_style_text_color(title, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 10, 34);

    for (int i = 0; i < 6; i++) {
        lv_obj_t *btn = lv_btn_create(scr);
        lv_obj_set_size(btn, 200, 36);
        lv_obj_set_pos(btn, 20, 70 + i * 40);
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
