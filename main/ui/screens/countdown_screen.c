#include "countdown_screen.h"
#include <stdio.h>
#include <string.h>

#define BG       lv_color_hex(0x0A0A0A)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREY_BG  lv_color_hex(0x333333)

static lv_obj_t *time_lbl;
static lv_obj_t *preset_btns[4];
static lv_obj_t *min_lbl, *start_btn, *reset_btn;
static lv_obj_t *start_btn_lbl;

/* Countdown state */
static int remaining_sec = 1800;    /* default 30:00 */
static bool running = false;
static int cur_preset = 2;           /* 0=5min, 1=10min, 2=30min, 3=60min */
static lv_timer_t *countdown_timer = NULL;

static const int preset_mins[] = {5, 10, 30, 60};

/* ── Internal helpers ── */
static void update_time_display(void) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", remaining_sec / 60, remaining_sec % 60);
    lv_label_set_text(time_lbl, buf);
}

static void update_min_label(void) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", remaining_sec / 60);
    lv_label_set_text(min_lbl, buf);
}

static void update_preset_highlight(void) {
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_color(preset_btns[i],
            i == cur_preset ? ORANGE : GREY_BG, 0);
    }
}

static void set_preset(int idx) {
    if (idx < 0 || idx >= 4) return;
    cur_preset = idx;
    remaining_sec = preset_mins[idx] * 60;
    update_time_display();
    update_min_label();
    update_preset_highlight();
}

/* ── Callbacks ── */
static void preset_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!running) set_preset(i);
}

static void minus_cb(lv_event_t *e) {
    if (running) return;
    int m = remaining_sec / 60;
    if (m > 1) {
        remaining_sec = (m - 1) * 60;
        update_time_display();
        update_min_label();
    }
}

static void plus_cb(lv_event_t *e) {
    if (running) return;
    int m = remaining_sec / 60;
    if (m < 99) {
        remaining_sec = (m + 1) * 60;
        update_time_display();
        update_min_label();
    }
}

static void on_tick(lv_timer_t *t) {
    if (!running) return;
    if (remaining_sec > 0) {
        remaining_sec--;
        update_time_display();
    }
    if (remaining_sec == 0) {
        running = false;
        lv_label_set_text(start_btn_lbl, "\xE5\xBC\x80\xE5\xA7\x8B"); /* 开始 */
        lv_obj_set_style_bg_color(start_btn, ORANGE, 0);
        lv_timer_pause(countdown_timer);
    }
}

static void start_cb(lv_event_t *e) {
    if (remaining_sec <= 0) {
        set_preset(cur_preset);
        return;
    }
    running = !running;
    if (running) {
        lv_label_set_text(start_btn_lbl, "\xE6\x9A\x82\xE5\x81\x9C"); /* 暂停 */
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xE64A19), 0);
        lv_timer_resume(countdown_timer);
    } else {
        lv_label_set_text(start_btn_lbl, "\xE5\xBC\x80\xE5\xA7\x8B"); /* 开始 */
        lv_obj_set_style_bg_color(start_btn, ORANGE, 0);
        lv_timer_pause(countdown_timer);
    }
}

static void reset_cb(lv_event_t *e) {
    running = false;
    lv_timer_pause(countdown_timer);
    set_preset(cur_preset);
    lv_label_set_text(start_btn_lbl, "\xE5\xBC\x80\xE5\xA7\x8B"); /* 开始 */
    lv_obj_set_style_bg_color(start_btn, ORANGE, 0);
}

/* ── Screen create ── */
lv_obj_t *countdown_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);

    /* Status bar */
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 240, 28);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, BG, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_t *wifi = lv_label_create(sb);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_pos(wifi, 12, 6);
    lv_obj_t *bat = lv_label_create(sb);
    lv_label_set_text(bat, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(bat, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_pos(bat, 210, 6);

    /* Title */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "\xE5\x80\x92\xE8\xAE\xA1\xE6\x97\xB6"); /* 倒计时 */
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 15, 34);

    /* Time display */
    time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "30:00");
    lv_obj_set_style_text_color(time_lbl, ORANGE, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_center(time_lbl);
    lv_obj_set_y(time_lbl, 85);

    /* Preset row */
    int preset_x[] = {16, 66, 116, 166};
    for (int i = 0; i < 4; i++) {
        preset_btns[i] = lv_btn_create(scr);
        lv_obj_set_size(preset_btns[i], 46, 28);
        lv_obj_set_pos(preset_btns[i], preset_x[i], 150);
        lv_obj_set_style_bg_color(preset_btns[i], i == 2 ? ORANGE : GREY_BG, 0);
        lv_obj_set_style_radius(preset_btns[i], 8, 0);
        lv_obj_set_style_border_width(preset_btns[i], 0, 0);
        lv_obj_set_style_shadow_width(preset_btns[i], 0, 0);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d\xE5\x88\x86", preset_mins[i]); /* N分 */
        lv_obj_t *l = lv_label_create(preset_btns[i]);
        lv_label_set_text(l, buf);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFFFF), 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(preset_btns[i], preset_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    /* Adjustment row:  −  30  分  + */
    lv_obj_t *minus_btn = lv_btn_create(scr);
    lv_obj_set_size(minus_btn, 36, 32);
    lv_obj_set_pos(minus_btn, 55, 190);
    lv_obj_set_style_bg_color(minus_btn, GREY_BG, 0);
    lv_obj_set_style_radius(minus_btn, 8, 0);
    lv_obj_set_style_border_width(minus_btn, 0, 0);
    lv_obj_set_style_shadow_width(minus_btn, 0, 0);
    lv_obj_t *m_l = lv_label_create(minus_btn);
    lv_label_set_text(m_l, "\xE2\x88\x92"); /* − */
    lv_obj_set_style_text_color(m_l, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_center(m_l);
    lv_obj_add_event_cb(minus_btn, minus_cb, LV_EVENT_CLICKED, NULL);

    min_lbl = lv_label_create(scr);
    lv_label_set_text(min_lbl, "30");
    lv_obj_set_style_text_color(min_lbl, lv_color_hex(0xFFFFFFFF), 0);
    lv_obj_set_style_text_font(min_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(min_lbl, 106, 192);

    lv_obj_t *unit_lbl = lv_label_create(scr);
    lv_label_set_text(unit_lbl, "\xE5\x88\x86"); /* 分 */
    lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(unit_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(unit_lbl, 138, 198);

    lv_obj_t *plus_btn = lv_btn_create(scr);
    lv_obj_set_size(plus_btn, 36, 32);
    lv_obj_set_pos(plus_btn, 155, 190);
    lv_obj_set_style_bg_color(plus_btn, GREY_BG, 0);
    lv_obj_set_style_radius(plus_btn, 8, 0);
    lv_obj_set_style_border_width(plus_btn, 0, 0);
    lv_obj_set_style_shadow_width(plus_btn, 0, 0);
    lv_obj_t *p_l = lv_label_create(plus_btn);
    lv_label_set_text(p_l, "+");
    lv_obj_set_style_text_color(p_l, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_center(p_l);
    lv_obj_add_event_cb(plus_btn, plus_cb, LV_EVENT_CLICKED, NULL);

    /* Control buttons */
    start_btn = lv_btn_create(scr);
    lv_obj_set_size(start_btn, 80, 42);
    lv_obj_set_pos(start_btn, 30, 230);
    lv_obj_set_style_bg_color(start_btn, ORANGE, 0);
    lv_obj_set_style_radius(start_btn, 10, 0);
    lv_obj_set_style_border_width(start_btn, 0, 0);
    lv_obj_set_style_shadow_width(start_btn, 0, 0);
    start_btn_lbl = lv_label_create(start_btn);
    lv_label_set_text(start_btn_lbl, "\xE5\xBC\x80\xE5\xA7\x8B"); /* 开始 */
    lv_obj_set_style_text_color(start_btn_lbl, lv_color_hex(0xFFFFFFFF), 0);
    lv_obj_set_style_text_font(start_btn_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(start_btn_lbl);
    lv_obj_add_event_cb(start_btn, start_cb, LV_EVENT_CLICKED, NULL);

    reset_btn = lv_btn_create(scr);
    lv_obj_set_size(reset_btn, 80, 42);
    lv_obj_set_pos(reset_btn, 130, 230);
    lv_obj_set_style_bg_color(reset_btn, GREY_BG, 0);
    lv_obj_set_style_radius(reset_btn, 10, 0);
    lv_obj_set_style_border_width(reset_btn, 0, 0);
    lv_obj_set_style_shadow_width(reset_btn, 0, 0);
    lv_obj_t *r_l = lv_label_create(reset_btn);
    lv_label_set_text(r_l, "\xE9\x87\x8D\xE7\xBD\xAE"); /* 重置 */
    lv_obj_set_style_text_color(r_l, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_style_text_font(r_l, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(r_l);
    lv_obj_add_event_cb(reset_btn, reset_cb, LV_EVENT_CLICKED, NULL);

    /* Countdown timer (1 sec tick, starts paused) */
    countdown_timer = lv_timer_create(on_tick, 1000, NULL);
    lv_timer_pause(countdown_timer);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

/* ── Public API ── */
void countdown_screen_set_time(int minutes, int seconds) {
    remaining_sec = minutes * 60 + seconds;
    if (remaining_sec < 0) remaining_sec = 0;
    update_time_display();
    update_min_label();
}

void countdown_screen_set_state(bool is_running, int preset_idx) {
    running = is_running;
    if (preset_idx >= 0 && preset_idx < 4) cur_preset = preset_idx;
    update_preset_highlight();
    if (running) {
        lv_timer_resume(countdown_timer);
        lv_label_set_text(start_btn_lbl, "\xE6\x9A\x82\xE5\x81\x9C"); /* 暂停 */
        lv_obj_set_style_bg_color(start_btn, lv_color_hex(0xE64A19), 0);
    } else {
        lv_timer_pause(countdown_timer);
        lv_label_set_text(start_btn_lbl, "\xE5\xBC\x80\xE5\xA7\x8B"); /* 开始 */
        lv_obj_set_style_bg_color(start_btn, ORANGE, 0);
    }
}

void countdown_screen_update(void) {
    update_time_display();
    update_min_label();
    update_preset_highlight();
}
