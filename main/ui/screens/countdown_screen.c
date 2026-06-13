#include "countdown_screen.h"
#include "ui/components/status_bar.h"
#include "ui/display_driver.h"
#include "modules/imu/imu_driver.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
static const char *TAG = "TIMER";

#define BG       lv_color_hex(0x0A0A0A)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREY     lv_color_hex(0x999999)
#define WHITE    lv_color_hex(0xFFFFFF)

static lv_obj_t *time_lbl, *phase_lbl, *status_lbl;
static int remaining = 30 * 60;
static int total_sec = 30 * 60;
static bool running = false;
static bool rest_mode = false;
static lv_timer_t *timer = NULL;
static int startup = 0;
static int side = 0;
static int side_debounce = 0;

/* 屏幕删除回调: 清理定时器 */
static void scr_del_cb(lv_event_t *e) {
    (void)e;
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

static void update_display(void) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", remaining / 60, remaining % 60);
    if (time_lbl) lv_label_set_text(time_lbl, buf);
}

static void on_tick(lv_timer_t *t) {
    if (startup < 3) { startup++; return; }

    /* 检测左右横置: 稳定侧躺才算, 晃动/倾斜不算 */
    int new_side = 0;
    imu_data_t imu;
    if (imu_read(&imu) == ESP_OK) {
        float ax = imu.ax, ay = imu.ay, az = imu.az;
        ESP_LOGI(TAG, "ax=%.2f ay=%.2f az=%.2f", ax, ay, az);
        /* 横置检测: 无论绕X轴还是Y轴翻转, 只要az<0.7且某一水平轴>0.7 */
        if (az > -0.7f && az < 0.7f) {
            if (ax > 0.7f) new_side = 1;
            else if (ax < -0.7f) new_side = -1;
            else if (ay > 0.7f) new_side = 1;
            else if (ay < -0.7f) new_side = -1;
        }
    }

    /* 消抖: 连续2秒相同才确认 */
    if (new_side == side) side_debounce++;
    else { side_debounce = 0; side = new_side; }

    if (side_debounce >= 1) {
        if (side == 1 && !running) {
            rest_mode = false; total_sec = 30 * 60; remaining = total_sec;
            running = true;
            if (phase_lbl) lv_label_set_text(phase_lbl, "久坐提醒");
            if (status_lbl) lv_label_set_text(status_lbl, "工作中");
            lv_timer_resume(timer); update_display();
        } else if (side == -1 && !running) {
            rest_mode = true; total_sec = 5 * 60; remaining = total_sec;
            running = true;
            if (phase_lbl) lv_label_set_text(phase_lbl, "休息时间");
            if (status_lbl) lv_label_set_text(status_lbl, "休息中");
            lv_timer_resume(timer); update_display();
        }
    }

    if (!running) return;
    if (remaining > 0) { remaining--; update_display(); }
    if (remaining == 0) { running = false; lv_timer_pause(timer); if (status_lbl) lv_label_set_text(status_lbl, "时间到!"); }
}

lv_obj_t *countdown_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_add_event_cb(scr, scr_del_cb, LV_EVENT_DELETE, NULL);
    status_bar_create(scr);

    phase_lbl = lv_label_create(scr);
    lv_label_set_text(phase_lbl, "久坐提醒");
    lv_obj_set_style_text_color(phase_lbl, WHITE, 0);
    lv_obj_set_style_text_font(phase_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(phase_lbl, 15, 40);

    time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "30:00");
    lv_obj_set_style_text_color(time_lbl, ORANGE, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_center(time_lbl);

    status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, "右倾30分/左倾5分");
    lv_obj_set_style_text_color(status_lbl, GREY, 0);
    lv_obj_set_style_text_font(status_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(status_lbl, 0, 230);
    lv_obj_set_width(status_lbl, 240);
    lv_obj_set_style_text_align(status_lbl, LV_TEXT_ALIGN_CENTER, 0);

    timer = lv_timer_create(on_tick, 1000, NULL);
    lv_timer_pause(timer);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);

    startup = 0;
    running = false;
    side = 0;
    side_debounce = 0;
    lv_timer_resume(timer);

    return scr;
}

void countdown_screen_set_time(int m, int s) { total_sec = m*60+s; remaining = total_sec; update_display(); }
void countdown_screen_set_state(bool run, int idx) { running = run; if(run)lv_timer_resume(timer); else lv_timer_pause(timer); }
void countdown_screen_update(void) { update_display(); }

void countdown_screen_reset(void) {
    running = false;
    rest_mode = false;
    remaining = 30 * 60;
    total_sec = 30 * 60;
    side = 0;
    side_debounce = 0;
    startup = 0;
    if (timer) {
        lv_timer_pause(timer);
        lv_timer_resume(timer);
    }
    if (phase_lbl) lv_label_set_text(phase_lbl, "久坐提醒");
    if (time_lbl) lv_label_set_text(time_lbl, "30:00");
    if (status_lbl) lv_label_set_text(status_lbl, "右倾30分/左倾5分");
}
