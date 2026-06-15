#include "countdown_screen.h"
#include "ui/display_driver.h"
#include "modules/imu/imu_driver.h"
#include "modules/audio/box_audio_codec.h"
#include "bsp/st7789_driver.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
static const char *TAG = "TIMER";

#define BG       lv_color_hex(0x0A0A0A)
#define ORANGE   lv_color_hex(0xFF5C00)

static lv_obj_t *time_lbl;
static int remaining = 30 * 60;
static int total_sec = 30 * 60;
static bool running = false;
static bool rest_mode = false;
static lv_timer_t *timer = NULL;
static int startup = 0;
static int side = 0;
static int side_debounce = 0;
static int current_rot = 0;  /* 0=NONE, -1=LEFT, 1=RIGHT */

static void scr_del_cb(lv_event_t *e) {
    (void)e;
    if (timer) { lv_timer_del(timer); timer = NULL; }
    esp_lcd_panel_handle_t p = st7789_get_panel();
    if (p) { esp_lcd_panel_swap_xy(p, false); esp_lcd_panel_mirror(p, false, false); esp_lcd_panel_set_gap(p, 0, 20); }
}

static void update_display(void) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", remaining / 60, remaining % 60);
    if (time_lbl) lv_label_set_text(time_lbl, buf);
}

static void set_hw_rot(int rot) {
    esp_lcd_panel_handle_t p = st7789_get_panel();
    if (!p) return;
    if (rot == -1) { /* 左躺 */
        esp_lcd_panel_swap_xy(p, true);
        esp_lcd_panel_mirror(p, true, false);
        esp_lcd_panel_set_gap(p, 0, 0);
    } else if (rot == 1) { /* 右躺 */
        esp_lcd_panel_swap_xy(p, true);
        esp_lcd_panel_mirror(p, false, true);
        esp_lcd_panel_set_gap(p, 0, 0);
    } else { /* 回正 */
        esp_lcd_panel_swap_xy(p, false);
        esp_lcd_panel_mirror(p, false, false);
        esp_lcd_panel_set_gap(p, 0, 20);
    }
    st7789_clear();
    if (rot == 0) {
        lv_obj_center(time_lbl);
    } else {
        lv_obj_align(time_lbl, LV_ALIGN_CENTER, 36, -28);
    }
    lv_obj_invalidate(lv_scr_act());
}

static void on_tick(lv_timer_t *t) {
    if (startup < 1) { startup++; return; }

    int new_side = 0;
    imu_data_t imu;
    if (imu_read(&imu) == ESP_OK) {
        float ay = imu.ay;
        ESP_LOGI(TAG, "ax=%.2f ay=%.2f az=%.2f", imu.ax, ay, imu.az);

        if      (ay < -0.9f) new_side = -1;  /* 左侧躺 */
        else if (ay >  0.9f) new_side =  1;  /* 右侧躺 */

        int new_rot = 0;
        if      (ay < -0.9f) new_rot = -1;
        else if (ay >  0.9f) new_rot =  1;
        if (new_rot != current_rot) {
            set_hw_rot(new_rot);
            ESP_LOGI(TAG, "ROT %d", new_rot);
            current_rot = new_rot;
        }
    }

    /* 消抖 */
    if (new_side == side) side_debounce++;
    else { side_debounce = 0; side = new_side; }

    if (side_debounce >= 1) {
        if (side == -1 && !running) {
            rest_mode = true; total_sec = 5 * 60; remaining = total_sec;
            running = true;
            lv_timer_resume(timer); update_display();
        } else if (side == 1 && !running) {
            rest_mode = false; total_sec = 30 * 60; remaining = total_sec;
            running = true;
            lv_timer_resume(timer); update_display();
        }
    }

    if (!running) return;
    if (remaining > 0) { remaining--; update_display(); }
    if (remaining == 0) {
        running = false;
        lv_timer_pause(timer);
        if (time_lbl) lv_label_set_text(time_lbl, "\xE6\x97\xB6\xE9\x97\xB4\xE5\x88\xB0\x21");  /* 时间到! */
        box_audio_beep();
    }
}

lv_obj_t *countdown_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_add_event_cb(scr, scr_del_cb, LV_EVENT_DELETE, NULL);

    time_lbl = lv_label_create(scr);
    lv_label_set_text(time_lbl, "30:00");
    lv_obj_set_style_text_color(time_lbl, ORANGE, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_48, 0);
    lv_obj_center(time_lbl);

    timer = lv_timer_create(on_tick, 1000, NULL);
    lv_timer_pause(timer);

    lv_scr_load(scr);
    if (old) lv_obj_del(old);

    current_rot = 0;
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

bool countdown_screen_is_finished(void) {
    return remaining == 0 && !running;
}

void countdown_screen_reset(void) {
    running = false;
    rest_mode = false;
    remaining = 30 * 60;
    total_sec = 30 * 60;
    side = 0;
    side_debounce = 0;
    startup = 0;
    if (current_rot != 0) { set_hw_rot(0); current_rot = 0; }
    if (timer) { lv_timer_pause(timer); lv_timer_resume(timer); }
    if (time_lbl) lv_label_set_text(time_lbl, "30:00");
}
