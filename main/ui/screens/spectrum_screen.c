#include "spectrum_screen.h"
#include "ui/components/status_bar.h"
#include "ui/components/theme_colors.h"
#include "modules/spectrum/spectrum_analyzer.h"
#include "modules/audio/box_audio_codec.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPECTRUM";

#define BAR_W      13
#define BAR_GAP     2
#define BAR_MAX_H  160
#define BAR_Y      96
#define BAR_START_X ((240 - SPECTRUM_BARS * (BAR_W + BAR_GAP) + BAR_GAP) / 2)

static lv_obj_t *s_bars[SPECTRUM_BARS];
static lv_obj_t *s_peaks[SPECTRUM_BARS];
static lv_obj_t *s_scr = NULL;
static lv_timer_t *s_timer = NULL;

/* 平滑值：当前显示高度（带缓动） */
static uint16_t s_smooth[SPECTRUM_BARS];
static uint16_t s_peak_val[SPECTRUM_BARS];
static int s_peak_timer[SPECTRUM_BARS];   /* 峰值保持帧数 */

/* ── 根据幅度获取颜色：绿→黄→红 ── */
static lv_color_t bar_color(uint16_t mag) {
    if (mag < 1024) {
        /* 绿 → 黄： R 从 0→255, G 保持 255 */
        int r = mag * 255 / 1024;
        return lv_color_make((uint8_t)r, 255, 0);
    } else if (mag < 2048) {
        /* 黄 → 红： G 从 255→0, R 保持 255 */
        int g = 255 - (mag - 1024) * 255 / 1024;
        return lv_color_make(255, (uint8_t)g, 0);
    } else {
        return lv_color_make(255, 0, 56);  /* 深红 */
    }
}

/* ── 更新一组柱 ── */
static void update_bars(const spectrum_result_t *res) {
    for (int i = 0; i < SPECTRUM_BARS; i++) {
        /* 平滑：指数移动平均 */
        s_smooth[i] = (s_smooth[i] * 3 + res->bands[i]) / 4;

        /* 峰值保持 */
        if (s_smooth[i] > s_peak_val[i]) {
            s_peak_val[i] = s_smooth[i];
            s_peak_timer[i] = 15;  /* 保持 ~750ms */
        } else if (s_peak_timer[i] > 0) {
            s_peak_timer[i]--;
        } else {
            s_peak_val[i] = s_peak_val[i] * 7 / 8;  /* 缓慢下落 */
        }

        /* 柱高度：映射 0-4095 → 0-BAR_MAX_H */
        int h = (int)s_smooth[i] * BAR_MAX_H / 4095;
        if (h < 2) h = 2;
        if (h > BAR_MAX_H) h = BAR_MAX_H;

        lv_obj_set_size(s_bars[i], BAR_W, h);
        lv_obj_set_pos(s_bars[i], BAR_START_X + i * (BAR_W + BAR_GAP), BAR_Y + BAR_MAX_H - h);
        lv_obj_set_style_bg_color(s_bars[i], bar_color(s_smooth[i]), 0);

        /* 峰值点 */
        int ph = (int)s_peak_val[i] * BAR_MAX_H / 4095;
        if (ph < 2) ph = 2;
        if (ph > BAR_MAX_H) ph = BAR_MAX_H;
        lv_obj_set_size(s_peaks[i], BAR_W, 3);
        lv_obj_set_pos(s_peaks[i], BAR_START_X + i * (BAR_W + BAR_GAP), BAR_Y + BAR_MAX_H - ph);
    }
}

/* ── 定时器回调 ── */
static void on_tick(lv_timer_t *t) {
    (void)t;
    /* 从麦克风采集 256 采样 → FFT */
    int16_t buf[FFT_N];
    esp_err_t r = box_audio_read(buf, FFT_N);
    if (r != ESP_OK) {
        /* 静音时填充零 */
        memset(buf, 0, sizeof(buf));
    }
    spectrum_result_t res;
    spectrum_analyze(buf, FFT_N, &res);
    update_bars(&res);
}

/* ── 屏幕删除回调 ── */
static void scr_del_cb(lv_event_t *e) {
    (void)e;
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    for (int i = 0; i < SPECTRUM_BARS; i++) {
        s_bars[i] = NULL;
        s_peaks[i] = NULL;
    }
    s_scr = NULL;
}

/* ── 创建 ── */
lv_obj_t *spectrum_screen_create(void) {
    /* 重置静态 */
    s_timer = NULL;
    s_scr = NULL;
    memset(s_smooth, 0, sizeof(s_smooth));
    memset(s_peak_val, 0, sizeof(s_peak_val));
    memset(s_peak_timer, 0, sizeof(s_peak_timer));
    for (int i = 0; i < SPECTRUM_BARS; i++) {
        s_bars[i] = NULL;
        s_peaks[i] = NULL;
    }
    spectrum_init();

    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, CLR_BG, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_add_event_cb(s_scr, scr_del_cb, LV_EVENT_DELETE, NULL);

    status_bar_create(s_scr);

    /* 标题 */
    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, "频谱");
    lv_obj_set_style_text_color(title, CLR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 15, 36);

    /* 16 根频谱柱 */
    for (int i = 0; i < SPECTRUM_BARS; i++) {
        /* 主柱体 */
        s_bars[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_bars[i], BAR_W, 2);
        lv_obj_set_pos(s_bars[i], BAR_START_X + i * (BAR_W + BAR_GAP), BAR_Y + BAR_MAX_H - 2);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_make(0, 255, 0), 0);
        lv_obj_set_style_radius(s_bars[i], 2, 0);
        lv_obj_set_style_border_width(s_bars[i], 0, 0);
        lv_obj_set_style_pad_all(s_bars[i], 0, 0);

        /* 峰值点 */
        s_peaks[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_peaks[i], BAR_W, 3);
        lv_obj_set_pos(s_peaks[i], BAR_START_X + i * (BAR_W + BAR_GAP), BAR_Y + BAR_MAX_H - 2);
        lv_obj_set_style_bg_color(s_peaks[i], CLR_TEXT, 0);
        lv_obj_set_style_radius(s_peaks[i], 1, 0);
        lv_obj_set_style_border_width(s_peaks[i], 0, 0);
        lv_obj_set_style_pad_all(s_peaks[i], 0, 0);
    }

    /* 提示 */
    lv_obj_t *hint = lv_label_create(s_scr);
    lv_label_set_text(hint, "B/START = 退出");
    lv_obj_set_style_text_color(hint, CLR_SUBTEXT, 0);
    lv_obj_set_style_text_font(hint, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);

    /* 50ms 定时器 = 20fps 刷新率 */
    s_timer = lv_timer_create(on_tick, 50, NULL);

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    return s_scr;
}
