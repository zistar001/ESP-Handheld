#include "countdown_screen.h"
#include "ui/components/status_bar.h"
#include <stdio.h>
#include <string.h>

#define BG       lv_color_hex(0x0A0A0A)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREY_BG  lv_color_hex(0x333333)
#define WHITE    lv_color_hex(0xFFFFFF)
#define GREY     lv_color_hex(0x999999)

/* Tab state */
static int s_tab = 0; /* 0=alarm, 1=pomodoro */
static lv_obj_t *s_tab_alarm, *s_tab_pomodoro;
static lv_obj_t *s_alarm_content, *s_pomodoro_content;

/* Alarm mode */
static lv_obj_t *alarm_time_lbl;
static int alarm_hour = 7, alarm_min = 30;

/* Pomodoro mode */
static lv_obj_t *pomo_time_lbl;
static lv_obj_t *pomo_start_btn, *pomo_start_lbl;
static int pomo_remaining = 25 * 60;
static bool pomo_running = false;
static lv_timer_t *pomo_timer = NULL;
static int pomo_preset = 0; /* 0=25min */

static const int pomo_presets[] = {25, 5, 15};

/* ── Tab switching ── */
static void update_tab_style(void) {
    lv_obj_set_style_bg_color(s_tab_alarm, s_tab == 0 ? ORANGE : GREY_BG, 0);
    lv_obj_set_style_bg_color(s_tab_pomodoro, s_tab == 1 ? ORANGE : GREY_BG, 0);
    if (s_alarm_content) {
        if (s_tab == 0) lv_obj_clear_flag(s_alarm_content, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_alarm_content, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_pomodoro_content) {
        if (s_tab == 1) lv_obj_clear_flag(s_pomodoro_content, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_pomodoro_content, LV_OBJ_FLAG_HIDDEN);
    }
}

static void tab_alarm_cb(lv_event_t *e) {
    (void)e;
    s_tab = 0;
    update_tab_style();
}

static void tab_pomo_cb(lv_event_t *e) {
    (void)e;
    s_tab = 1;
    update_tab_style();
}

/* ── Alarm callbacks ── */
static void alarm_minus_cb(lv_event_t *e) {
    (void)e;
    alarm_min -= 5;
    if (alarm_min < 0) { alarm_min = 55; alarm_hour--; }
    if (alarm_hour < 0) alarm_hour = 23;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarm_hour, alarm_min);
    lv_label_set_text(alarm_time_lbl, buf);
}

static void alarm_plus_cb(lv_event_t *e) {
    (void)e;
    alarm_min += 5;
    if (alarm_min >= 60) { alarm_min = 0; alarm_hour++; }
    if (alarm_hour >= 24) alarm_hour = 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarm_hour, alarm_min);
    lv_label_set_text(alarm_time_lbl, buf);
}

/* ── Pomodoro callbacks ── */
static void pomo_update_display(void) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", pomo_remaining / 60, pomo_remaining % 60);
    lv_label_set_text(pomo_time_lbl, buf);
}

static void pomo_tick(lv_timer_t *t) {
    if (!pomo_running) return;
    if (pomo_remaining > 0) {
        pomo_remaining--;
        pomo_update_display();
    }
    if (pomo_remaining == 0) {
        pomo_running = false;
        lv_label_set_text(pomo_start_lbl, "\xe5\xbc\x80\xe5\xa7\x8b"); /* 开始 */
        lv_obj_set_style_bg_color(pomo_start_btn, ORANGE, 0);
        lv_timer_pause(pomo_timer);
    }
}

static void pomo_start_cb(lv_event_t *e) {
    (void)e;
    if (pomo_remaining <= 0) {
        pomo_remaining = pomo_presets[pomo_preset] * 60;
        pomo_update_display();
    }
    pomo_running = !pomo_running;
    if (pomo_running) {
        lv_label_set_text(pomo_start_lbl, "\xe6\x9a\x82\xe5\x81\x9c"); /* 暂停 */
        lv_obj_set_style_bg_color(pomo_start_btn, lv_color_hex(0xE64A19), 0);
        lv_timer_resume(pomo_timer);
    } else {
        lv_label_set_text(pomo_start_lbl, "\xe5\xbc\x80\xe5\xa7\x8b"); /* 开始 */
        lv_obj_set_style_bg_color(pomo_start_btn, ORANGE, 0);
        lv_timer_pause(pomo_timer);
    }
}

static void pomo_reset_cb(lv_event_t *e) {
    (void)e;
    pomo_running = false;
    lv_timer_pause(pomo_timer);
    pomo_remaining = pomo_presets[pomo_preset] * 60;
    pomo_update_display();
    lv_label_set_text(pomo_start_lbl, "\xe5\xbc\x80\xe5\xa7\x8b"); /* 开始 */
    lv_obj_set_style_bg_color(pomo_start_btn, ORANGE, 0);
}

static void pomo_preset_cb(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (pomo_running) return;
    pomo_preset = idx;
    pomo_remaining = pomo_presets[idx] * 60;
    pomo_update_display();
}

lv_obj_t *countdown_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);

    /* Status bar */
    status_bar_create(scr);

    /* Tab Bar (Pencil: y=32, height=32) */
    s_tab_alarm = lv_obj_create(scr);
    lv_obj_set_size(s_tab_alarm, 120, 32);
    lv_obj_set_pos(s_tab_alarm, 0, 32);
    lv_obj_set_style_bg_color(s_tab_alarm, ORANGE, 0);
    lv_obj_set_style_border_width(s_tab_alarm, 0, 0);
    lv_obj_set_style_radius(s_tab_alarm, 0, 0);
    lv_obj_set_style_pad_all(s_tab_alarm, 0, 0);
    lv_obj_add_flag(s_tab_alarm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tab_alarm, tab_alarm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tab1_lbl = lv_label_create(s_tab_alarm);
    lv_label_set_text(tab1_lbl, "\xe9\x97\xb9\xe9\x92\x9f"); /* 闹钟 */
    lv_obj_set_style_text_color(tab1_lbl, WHITE, 0);
    lv_obj_set_style_text_font(tab1_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(tab1_lbl);

    s_tab_pomodoro = lv_obj_create(scr);
    lv_obj_set_size(s_tab_pomodoro, 120, 32);
    lv_obj_set_pos(s_tab_pomodoro, 120, 32);
    lv_obj_set_style_bg_color(s_tab_pomodoro, GREY_BG, 0);
    lv_obj_set_style_border_width(s_tab_pomodoro, 0, 0);
    lv_obj_set_style_radius(s_tab_pomodoro, 0, 0);
    lv_obj_set_style_pad_all(s_tab_pomodoro, 0, 0);
    lv_obj_add_flag(s_tab_pomodoro, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tab_pomodoro, tab_pomo_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tab2_lbl = lv_label_create(s_tab_pomodoro);
    lv_label_set_text(tab2_lbl, "\xe8\xae\xa1\xe6\x97\xb6"); /* 计时 */
    lv_obj_set_style_text_color(tab2_lbl, WHITE, 0);
    lv_obj_set_style_text_font(tab2_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(tab2_lbl);

    /* ═══ Alarm Content ═══ */
    s_alarm_content = lv_obj_create(scr);
    lv_obj_set_size(s_alarm_content, 240, 248);
    lv_obj_set_pos(s_alarm_content, 0, 32);
    lv_obj_set_style_bg_opa(s_alarm_content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_alarm_content, 0, 0);
    lv_obj_set_style_radius(s_alarm_content, 0, 0);
    lv_obj_set_style_pad_all(s_alarm_content, 0, 0);

    /* Current time display */
    lv_obj_t *cur_time = lv_label_create(s_alarm_content);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarm_hour, alarm_min);
    lv_label_set_text(cur_time, buf);
    lv_obj_set_style_text_color(cur_time, WHITE, 0);
    lv_obj_set_style_text_font(cur_time, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(cur_time, 0, 28);
    lv_obj_set_width(cur_time, 240);
    lv_obj_set_style_text_align(cur_time, LV_TEXT_ALIGN_CENTER, 0);

    /* "设定闹钟" label */
    lv_obj_t *set_lbl = lv_label_create(s_alarm_content);
    lv_label_set_text(set_lbl, "\xe8\xae\xbe\xe5\xae\x9a\xe9\x97\xb9\xe9\x92\x9f"); /* 设定闹钟 */
    lv_obj_set_style_text_color(set_lbl, GREY, 0);
    lv_obj_set_style_text_font(set_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(set_lbl, 0, 83);
    lv_obj_set_width(set_lbl, 240);
    lv_obj_set_style_text_align(set_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* Time selector: − 07:30 + */
    lv_obj_t *minus_btn = lv_btn_create(s_alarm_content);
    lv_obj_set_size(minus_btn, 36, 32);
    lv_obj_set_pos(minus_btn, 35, 100);
    lv_obj_set_style_bg_color(minus_btn, GREY_BG, 0);
    lv_obj_set_style_radius(minus_btn, 8, 0);
    lv_obj_set_style_border_width(minus_btn, 0, 0);
    lv_obj_t *m_l = lv_label_create(minus_btn);
    lv_label_set_text(m_l, "\xe2\x88\x92"); /* − */
    lv_obj_set_style_text_color(m_l, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_center(m_l);
    lv_obj_add_event_cb(minus_btn, alarm_minus_cb, LV_EVENT_CLICKED, NULL);

    alarm_time_lbl = lv_label_create(s_alarm_content);
    lv_label_set_text(alarm_time_lbl, buf);
    lv_obj_set_style_text_color(alarm_time_lbl, WHITE, 0);
    lv_obj_set_style_text_font(alarm_time_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(alarm_time_lbl, 85, 102);

    lv_obj_t *plus_btn = lv_btn_create(s_alarm_content);
    lv_obj_set_size(plus_btn, 36, 32);
    lv_obj_set_pos(plus_btn, 170, 100);
    lv_obj_set_style_bg_color(plus_btn, GREY_BG, 0);
    lv_obj_set_style_radius(plus_btn, 8, 0);
    lv_obj_set_style_border_width(plus_btn, 0, 0);
    lv_obj_t *p_l = lv_label_create(plus_btn);
    lv_label_set_text(p_l, "+");
    lv_obj_set_style_text_color(p_l, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_center(p_l);
    lv_obj_add_event_cb(plus_btn, alarm_plus_cb, LV_EVENT_CLICKED, NULL);

    /* Repeat row */
    lv_obj_t *repeat_lbl = lv_label_create(s_alarm_content);
    lv_label_set_text(repeat_lbl, "\xe9\x87\x8d\xe5\xa4\x8d"); /* 重复 */
    lv_obj_set_style_text_color(repeat_lbl, WHITE, 0);
    lv_obj_set_style_text_font(repeat_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(repeat_lbl, 20, 143);

    lv_obj_t *repeat_val = lv_label_create(s_alarm_content);
    lv_label_set_text(repeat_val, "一次 >");
    lv_obj_set_style_text_color(repeat_val, GREY, 0);
    lv_obj_set_style_text_font(repeat_val, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(repeat_val, 140, 145);

    /* Toggle row */
    lv_obj_t *toggle_lbl = lv_label_create(s_alarm_content);
    lv_label_set_text(toggle_lbl, "\xe9\x97\xb9\xe9\x92\x9f"); /* 闹钟 */
    lv_obj_set_style_text_color(toggle_lbl, WHITE, 0);
    lv_obj_set_style_text_font(toggle_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(toggle_lbl, 20, 173);

    /* Toggle pill */
    lv_obj_t *toggle_pill = lv_obj_create(s_alarm_content);
    lv_obj_set_size(toggle_pill, 44, 24);
    lv_obj_set_pos(toggle_pill, 176, 173);
    lv_obj_set_style_bg_color(toggle_pill, ORANGE, 0);
    lv_obj_set_style_radius(toggle_pill, 12, 0);
    lv_obj_set_style_border_width(toggle_pill, 0, 0);
    lv_obj_set_style_pad_all(toggle_pill, 2, 0);
    lv_obj_t *knob = lv_obj_create(toggle_pill);
    lv_obj_set_size(knob, 20, 20);
    lv_obj_set_style_bg_color(knob, WHITE, 0);
    lv_obj_set_style_radius(knob, 10, 0);
    lv_obj_set_style_border_width(knob, 0, 0);
    lv_obj_align(knob, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Save button */
    lv_obj_t *save_btn = lv_btn_create(s_alarm_content);
    lv_obj_set_size(save_btn, 200, 36);
    lv_obj_set_pos(save_btn, 20, 208);
    lv_obj_set_style_bg_color(save_btn, ORANGE, 0);
    lv_obj_set_style_radius(save_btn, 8, 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "\xe4\xbf\x9d\xe5\xad\x98"); /* 保存 */
    lv_obj_set_style_text_color(save_lbl, WHITE, 0);
    lv_obj_set_style_text_font(save_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(save_lbl);

    /* ═══ Pomodoro Content ═══ */
    s_pomodoro_content = lv_obj_create(scr);
    lv_obj_set_size(s_pomodoro_content, 240, 248);
    lv_obj_set_pos(s_pomodoro_content, 0, 32);
    lv_obj_set_style_bg_opa(s_pomodoro_content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_pomodoro_content, 0, 0);
    lv_obj_set_style_radius(s_pomodoro_content, 0, 0);
    lv_obj_set_style_pad_all(s_pomodoro_content, 0, 0);
    lv_obj_add_flag(s_pomodoro_content, LV_OBJ_FLAG_HIDDEN);

    /* Timer display */
    pomo_time_lbl = lv_label_create(s_pomodoro_content);
    lv_label_set_text(pomo_time_lbl, "25:00");
    lv_obj_set_style_text_color(pomo_time_lbl, ORANGE, 0);
    lv_obj_set_style_text_font(pomo_time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(pomo_time_lbl, 0, 20);
    lv_obj_set_width(pomo_time_lbl, 240);
    lv_obj_set_style_text_align(pomo_time_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* Quick select buttons */
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(s_pomodoro_content);
        lv_obj_set_size(btn, 56, 28);
        lv_obj_set_pos(btn, 20 + i * 68, 85);
        lv_obj_set_style_bg_color(btn, i == 0 ? ORANGE : GREY_BG, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        char buf2[16];
        snprintf(buf2, sizeof(buf2), "%d\xe5\x88\x86", pomo_presets[i]); /* N分 */
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_text(l, buf2);
        lv_obj_set_style_text_color(l, WHITE, 0);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(btn, pomo_preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* Tomato count */
    lv_obj_t *count_lbl = lv_label_create(s_pomodoro_content);
    lv_label_set_text(count_lbl, "已进行 0 个计时");
    lv_obj_set_style_text_color(count_lbl, GREY, 0);
    lv_obj_set_style_text_font(count_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(count_lbl, 0, 125);
    lv_obj_set_width(count_lbl, 240);
    lv_obj_set_style_text_align(count_lbl, LV_TEXT_ALIGN_CENTER, 0);

    /* Start button */
    pomo_start_btn = lv_btn_create(s_pomodoro_content);
    lv_obj_set_size(pomo_start_btn, 120, 44);
    lv_obj_set_pos(pomo_start_btn, 60, 155);
    lv_obj_set_style_bg_color(pomo_start_btn, ORANGE, 0);
    lv_obj_set_style_radius(pomo_start_btn, 22, 0);
    lv_obj_set_style_border_width(pomo_start_btn, 0, 0);
    pomo_start_lbl = lv_label_create(pomo_start_btn);
    lv_label_set_text(pomo_start_lbl, "\xe5\xbc\x80\xe5\xa7\x8b"); /* 开始 */
    lv_obj_set_style_text_color(pomo_start_lbl, WHITE, 0);
    lv_obj_set_style_text_font(pomo_start_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(pomo_start_lbl);
    lv_obj_add_event_cb(pomo_start_btn, pomo_start_cb, LV_EVENT_CLICKED, NULL);

    /* Reset button */
    lv_obj_t *reset_btn = lv_btn_create(s_pomodoro_content);
    lv_obj_set_size(reset_btn, 80, 32);
    lv_obj_set_pos(reset_btn, 80, 210);
    lv_obj_set_style_bg_color(reset_btn, GREY_BG, 0);
    lv_obj_set_style_radius(reset_btn, 8, 0);
    lv_obj_set_style_border_width(reset_btn, 0, 0);
    lv_obj_t *r_l = lv_label_create(reset_btn);
    lv_label_set_text(r_l, "\xe9\x87\x8d\xe7\xbd\xae"); /* 重置 */
    lv_obj_set_style_text_color(r_l, lv_color_hex(0xFFFFFFCC), 0);
    lv_obj_set_style_text_font(r_l, &lv_font_simsun_16_cjk, 0);
    lv_obj_center(r_l);
    lv_obj_add_event_cb(reset_btn, pomo_reset_cb, LV_EVENT_CLICKED, NULL);

    /* Pomodoro timer */
    pomo_timer = lv_timer_create(pomo_tick, 1000, NULL);
    lv_timer_pause(pomo_timer);

    /* Show alarm tab by default */
    update_tab_style();

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void countdown_screen_set_time(int minutes, int seconds) {
    pomo_remaining = minutes * 60 + seconds;
    if (pomo_remaining < 0) pomo_remaining = 0;
    pomo_update_display();
}

void countdown_screen_set_state(bool running, int preset_idx) {
    pomo_running = running;
    if (preset_idx >= 0 && preset_idx < 3) pomo_preset = preset_idx;
    if (pomo_running) {
        lv_timer_resume(pomo_timer);
        lv_label_set_text(pomo_start_lbl, "\xe6\x9a\x82\xe5\x81\x9c"); /* 暂停 */
        lv_obj_set_style_bg_color(pomo_start_btn, lv_color_hex(0xE64A19), 0);
    } else {
        lv_timer_pause(pomo_timer);
        lv_label_set_text(pomo_start_lbl, "\xe5\xbc\x80\xe5\xa7\x8b"); /* 开始 */
        lv_obj_set_style_bg_color(pomo_start_btn, ORANGE, 0);
    }
}

void countdown_screen_update(void) {
    pomo_update_display();
}
