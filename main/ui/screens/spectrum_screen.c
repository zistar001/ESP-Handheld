#include "spectrum_screen.h"
#include "ui/components/status_bar.h"
#include "ui/components/theme_colors.h"
#include "modules/spectrum/spectrum_analyzer.h"
#include "modules/audio/box_audio_codec.h"
#include "app/app_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SPECTRUM";

#define BAR_W       13
#define BAR_GAP     2
#define BAR_HALF    110
#define CENTER_Y    148
#define BAR_START_X ((240 - SPECTRUM_BARS * (BAR_W + BAR_GAP) + BAR_GAP) / 2)

static const char *s_param_names[] = {"\xe9\x97\xa8\xe9\x99\x90","\xe5\xb9\xb3\xe6\xbb\x91","\xe5\xa2\x9e\xe7\x9b\x8a"};
#define PARAM_COUNT 3

static lv_obj_t *s_hi[SPECTRUM_BARS], *s_lo[SPECTRUM_BARS];
static lv_obj_t *s_peak_hi[SPECTRUM_BARS], *s_peak_lo[SPECTRUM_BARS];
static lv_obj_t *s_scr = NULL, *s_param_lbl = NULL;
static lv_timer_t *s_timer = NULL;
static uint16_t s_smooth[SPECTRUM_BARS], s_peak_val[SPECTRUM_BARS];
static int s_peak_timer[SPECTRUM_BARS], s_sel_param = 0;

/* ── 幅度 → 颜色 ── */
static lv_color_t bar_color(uint16_t mag) {
    if (mag < 1024) { int r = mag * 255 / 1024; return lv_color_make((uint8_t)r, 255, 0); }
    else if (mag < 2048) { int g = 255 - (mag - 1024) * 255 / 1024; return lv_color_make(255, (uint8_t)g, 0); }
    else return lv_color_make(255, 0, 56);
}

/* ── 刷新参数标签 ── */
static void update_param_label(void) {
    if (!s_param_lbl) return;
    const spectrum_params_t *p = spectrum_get_params();
    char buf[48];
    const char *sel = s_param_names[s_sel_param];
    switch (s_sel_param) {
        case 0: snprintf(buf, sizeof(buf), "> %s: %.3f", sel, (double)p->noise_threshold); break;
        case 1: snprintf(buf, sizeof(buf), "> %s: %d", sel, p->smoothing); break;
        case 2: snprintf(buf, sizeof(buf), "> %s: %ddB", sel, p->mic_gain); break;
    }
    lv_label_set_text(s_param_lbl, buf);
}

/* ── 更新镜像柱 ── */
static void update_bars(const spectrum_result_t *res) {
    const spectrum_params_t *p = spectrum_get_params();
    int sm = p->smoothing;  /* 0=无平滑, 9=极慢 */
    int w_new = 10 - sm;    /* 新值权重: 10→1 */
    int w_old = sm;         /* 旧值权重: 0→9 */
    if (w_new < 1) w_new = 1;

    for (int i = 0; i < SPECTRUM_BARS; i++) {
        s_smooth[i] = (uint16_t)(((uint32_t)s_smooth[i] * w_old + (uint32_t)res->bands[i] * w_new) / 10);

        if (s_smooth[i] > s_peak_val[i]) { s_peak_val[i] = s_smooth[i]; s_peak_timer[i] = 15; }
        else if (s_peak_timer[i] > 0) s_peak_timer[i]--;
        else s_peak_val[i] = s_peak_val[i] * 7 / 8;

        int h = (int)s_smooth[i] * BAR_HALF / 3000;
        if (h < 1) h = 1;
        if (h > BAR_HALF) h = BAR_HALF;
        int x = BAR_START_X + i * (BAR_W + BAR_GAP);
        lv_color_t c = bar_color(s_smooth[i]);

        lv_obj_set_size(s_hi[i], BAR_W, h);
        lv_obj_set_pos(s_hi[i], x, CENTER_Y - h);
        lv_obj_set_style_bg_color(s_hi[i], c, 0);
        lv_obj_set_size(s_lo[i], BAR_W, h);
        lv_obj_set_pos(s_lo[i], x, CENTER_Y);
        lv_obj_set_style_bg_color(s_lo[i], c, 0);

        int ph = (int)s_peak_val[i] * BAR_HALF / 3000;
        if (ph < 1) ph = 1;
        if (ph > BAR_HALF) ph = BAR_HALF;
        lv_obj_set_pos(s_peak_hi[i], x, CENTER_Y - ph - 1);
        lv_obj_set_pos(s_peak_lo[i], x, CENTER_Y + ph - 2);
    }
}

/* ── 独立音频采集 + FFT 任务（栈安全，不占用 lv_tick 栈）─── */
static spectrum_result_t s_result;
static volatile bool s_result_ready = false;
static TaskHandle_t s_fft_task = NULL;

static void fft_task(void *arg) {
    /* 延迟等音频稳定 */
    vTaskDelay(pdMS_TO_TICKS(1000));

    int16_t *buf = heap_caps_malloc(FFT_N * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) { vTaskDelete(NULL); return; }

    while (1) {
        esp_err_t r = box_audio_read(buf, FFT_N);
        if (r != ESP_OK) memset(buf, 0, FFT_N * sizeof(int16_t));

        spectrum_result_t res;
        spectrum_analyze(buf, FFT_N, &res);

        /* 写入共享结果 */
        memcpy(&s_result, &res, sizeof(s_result));
        s_result_ready = true;

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/* ── LVGL 定时器：只负责取结果 + 刷新 UI ── */
static void on_tick(lv_timer_t *t) {
    (void)t;
    if (s_result_ready) {
        s_result_ready = false;
        update_bars(&s_result);
    }
}

/* ── 删除 ── */
static void scr_del_cb(lv_event_t *e) {
    (void)e;
    if (s_fft_task) { vTaskDelete(s_fft_task); s_fft_task = NULL; }
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    for (int i = 0; i < SPECTRUM_BARS; i++)
        s_hi[i] = s_lo[i] = s_peak_hi[i] = s_peak_lo[i] = NULL;
    s_scr = NULL; s_param_lbl = NULL;
    s_result_ready = false;
}

/* ── 按键导航 ── */
void spectrum_screen_navigate(int dir) {
    s_sel_param += dir;
    if (s_sel_param < 0) s_sel_param = 0;
    if (s_sel_param >= PARAM_COUNT) s_sel_param = PARAM_COUNT - 1;
    update_param_label();
}

void spectrum_screen_adjust(int dir) {
    const spectrum_params_t *p = spectrum_get_params();
    switch (s_sel_param) {
        case 0:
            spectrum_set_noise_threshold(p->noise_threshold + dir * 0.002f);
            break;
        case 1:
            spectrum_set_smoothing((uint8_t)((int)p->smoothing + dir));
            break;
        case 2: {
            int g = (int)p->mic_gain + dir * 3;
            if (g < 0) g = 0;
            if (g > 60) g = 60;
            spectrum_set_mic_gain_db((uint8_t)g);
            extern void box_audio_set_mic_gain(uint8_t db);
            box_audio_set_mic_gain((uint8_t)g);
            break;
        }
    }
    update_param_label();
}

/* ── 创建 ── */
lv_obj_t *spectrum_screen_create(void) {
    s_timer = NULL; s_scr = NULL; s_param_lbl = NULL; s_sel_param = 0;
    s_fft_task = NULL; s_result_ready = false;
    memset(s_smooth, 0, sizeof(s_smooth));
    memset(s_peak_val, 0, sizeof(s_peak_val));
    memset(s_peak_timer, 0, sizeof(s_peak_timer));
    for (int i = 0; i < SPECTRUM_BARS; i++)
        s_hi[i] = s_lo[i] = s_peak_hi[i] = s_peak_lo[i] = NULL;
    spectrum_init();
    extern void box_audio_set_mic_gain(uint8_t db);
    box_audio_set_mic_gain(37);

    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, CLR_BG, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_add_event_cb(s_scr, scr_del_cb, LV_EVENT_DELETE, NULL);
    status_bar_create(s_scr);

    /* 中心线 */
    lv_obj_t *cline = lv_obj_create(s_scr);
    lv_obj_set_size(cline, 240, 1); lv_obj_set_pos(cline, 0, CENTER_Y);
    lv_obj_set_style_bg_color(cline, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(cline, 0, 0);
    lv_obj_set_style_pad_all(cline, 0, 0);

    for (int i = 0; i < SPECTRUM_BARS; i++) {
        int x = BAR_START_X + i * (BAR_W + BAR_GAP);

        s_hi[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_hi[i], BAR_W, 2);
        lv_obj_set_style_radius(s_hi[i], 2, 0);
        lv_obj_set_style_border_width(s_hi[i], 0, 0);
        lv_obj_set_style_pad_all(s_hi[i], 0, 0);
        lv_obj_set_pos(s_hi[i], x, CENTER_Y - 2);
        lv_obj_set_style_bg_color(s_hi[i], lv_color_make(0, 255, 0), 0);

        s_lo[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_lo[i], BAR_W, 2);
        lv_obj_set_style_radius(s_lo[i], 2, 0);
        lv_obj_set_style_border_width(s_lo[i], 0, 0);
        lv_obj_set_style_pad_all(s_lo[i], 0, 0);
        lv_obj_set_pos(s_lo[i], x, CENTER_Y);
        lv_obj_set_style_bg_color(s_lo[i], lv_color_make(0, 255, 0), 0);

        s_peak_hi[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_peak_hi[i], BAR_W, 2);
        lv_obj_set_style_radius(s_peak_hi[i], 1, 0);
        lv_obj_set_style_border_width(s_peak_hi[i], 0, 0);
        lv_obj_set_style_pad_all(s_peak_hi[i], 0, 0);
        lv_obj_set_pos(s_peak_hi[i], x, CENTER_Y - 2);
        lv_obj_set_style_bg_color(s_peak_hi[i], CLR_TEXT, 0);

        s_peak_lo[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_peak_lo[i], BAR_W, 2);
        lv_obj_set_style_radius(s_peak_lo[i], 1, 0);
        lv_obj_set_style_border_width(s_peak_lo[i], 0, 0);
        lv_obj_set_style_pad_all(s_peak_lo[i], 0, 0);
        lv_obj_set_pos(s_peak_lo[i], x, CENTER_Y);
        lv_obj_set_style_bg_color(s_peak_lo[i], CLR_TEXT, 0);
    }

    /* 参数标签 */
    s_param_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_param_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_param_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(s_param_lbl, LV_ALIGN_BOTTOM_MID, 0, -3);
    update_param_label();

    s_timer = lv_timer_create(on_tick, 30, NULL);

    /* 独立任务做音频采集+FFT（栈安全，不抢占 lv_tick） */
    xTaskCreatePinnedToCore(fft_task, "fft_spec", 4096, NULL, 2, &s_fft_task, 1);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    return s_scr;
}
