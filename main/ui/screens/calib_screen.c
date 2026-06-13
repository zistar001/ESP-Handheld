#include "calib_screen.h"
#include "ui/components/status_bar.h"
#include "ui/display_driver.h"
#include "modules/imu/imu_driver.h"
#include "modules/imu/imu_calib.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
static const char *TAG = "CALIB";

#define BG       lv_color_hex(0x0A0A0A)
#define WHITE    lv_color_hex(0xFFFFFF)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREEN    lv_color_hex(0x00CC66)
#define GREY     lv_color_hex(0x666666)

static const char *POSE_PROMPTS[] = {
    "Step 1/4  UPRIGHT\nHold device vertically\nScreen facing you",
    "Step 2/4  LEFT SIDE\nLeft edge down\nPlace on table",
    "Step 3/4  RIGHT SIDE\nRight edge down\nPlace on table",
    "Step 4/4  FLAT\nScreen facing up\nPlace on table",
};
static const int POSE_COUNT = 4;

static lv_obj_t *step_lbl, *status_lbl;
static lv_obj_t *ax_lbl;
static int step = 0;
static imu_calib_t calib;
static bool sampling = false;
static int sample_count = 0;
static lv_timer_t *timer = NULL;

static void scr_del_cb(lv_event_t *e) {
    (void)e;
    if (timer) { lv_timer_del(timer); timer = NULL; }
}

static void add_sample(const imu_data_t *imu, imu_vec_t *v) {
    v->ax += imu->ax;
    v->ay += imu->ay;
    v->az += imu->az;
}

static void update_ui(void) {
    if (step < POSE_COUNT) {
        lv_label_set_text(step_lbl, POSE_PROMPTS[step]);
        lv_label_set_text(status_lbl, sampling ? "Sampling..." : "Press A to start");
    } else {
        lv_label_set_text(step_lbl, "Calibration done!");
        lv_label_set_text(status_lbl, "Saved to NVS\nPress B/START to exit");
    }
}

static void on_tick(lv_timer_t *t) {
    (void)t;
    imu_data_t imu;
    if (imu_read(&imu) != ESP_OK) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "ax: %.2f  ay: %.2f  az: %.2f", imu.ax, imu.ay, imu.az);
    if (ax_lbl) lv_label_set_text(ax_lbl, buf);

    if (sampling && step < POSE_COUNT) {
        add_sample(&imu, &calib.upright + step);
        sample_count++;
        char s[16];
        snprintf(s, sizeof(s), "%d/20", sample_count);
        if (status_lbl) lv_label_set_text(status_lbl, s);

        if (sample_count >= 20) {
            sampling = false;
            imu_vec_t *v = &calib.upright + step;
            v->ax /= 20.0f; v->ay /= 20.0f; v->az /= 20.0f;
            ESP_LOGI(TAG, "Pose %d: ax=%.2f ay=%.2f az=%.2f", step, v->ax, v->ay, v->az);
            step++;
            if (step >= POSE_COUNT) {
                calib.valid = true;
                imu_calib_save(&calib);
            }
            update_ui();
        }
    }
}

void calib_screen_trigger(void) {
    if (step >= POSE_COUNT) return;
    if (sampling) return;
    sampling = true;
    sample_count = 0;
    imu_vec_t *v = &calib.upright + step;
    v->ax = 0; v->ay = 0; v->az = 0;
    update_ui();
}

lv_obj_t *calib_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_add_event_cb(scr, scr_del_cb, LV_EVENT_DELETE, NULL);
    status_bar_create(scr);

    step = 0;
    sampling = false;
    sample_count = 0;
    memset(&calib, 0, sizeof(calib));

    step_lbl = lv_label_create(scr);
    lv_label_set_text(step_lbl, POSE_PROMPTS[0]);
    lv_obj_set_style_text_color(step_lbl, WHITE, 0);
    lv_obj_set_style_text_font(step_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(step_lbl, 20, 50);
    lv_obj_set_width(step_lbl, 200);
    lv_obj_set_style_text_align(step_lbl, LV_TEXT_ALIGN_CENTER, 0);

    ax_lbl = lv_label_create(scr);
    lv_label_set_text(ax_lbl, "ax: 0.00  ay: 0.00  az: 0.00");
    lv_obj_set_style_text_color(ax_lbl, GREY, 0);
    lv_obj_set_style_text_font(ax_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(ax_lbl, 20, 130);
    lv_obj_set_width(ax_lbl, 200);

    status_lbl = lv_label_create(scr);
    lv_label_set_text(status_lbl, "Press A to sample");
    lv_obj_set_style_text_color(status_lbl, ORANGE, 0);
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(status_lbl, LV_ALIGN_BOTTOM_MID, 0, -20);

    timer = lv_timer_create(on_tick, 100, NULL);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}
