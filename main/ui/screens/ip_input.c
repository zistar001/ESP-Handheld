#include "ip_input.h"
#include "modules/pc_remote/wifi_audio.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "ui/components/theme_colors.h"
#include <stdio.h>
#include <string.h>

#define BG     CLR_BG
#define ACCENT CLR_ACCENT
#define WHITE  CLR_TEXT
#define GREY   lv_color_hex(0x888888)
#define SUB    CLR_SUBTEXT

static lv_obj_t *prefix_lbl, *octet_lbl, *status_lbl;
static int octet_val = 1;

static void update_display(void) {
    char buf[32];
    /* Build prefix from ESP32's own IP */
    const char *my_ip = wifi_manager_get_ip();
    if (my_ip && strlen(my_ip) > 0) {
        char pfx[16]; strncpy(pfx, my_ip, sizeof(pfx)-1); pfx[sizeof(pfx)-1]='\0';
        char *dot = strrchr(pfx, '.');
        if (dot) *(dot+1) = '\0';  /* truncate after last dot: "192.168.3." */
        snprintf(buf, sizeof(buf), "%s", pfx);
        lv_label_set_text(prefix_lbl, buf);
    } else {
        lv_label_set_text(prefix_lbl, "x.x.x.");
    }
    /* Show current octet value */
    snprintf(buf, sizeof(buf), "%d", octet_val);
    lv_label_set_text(octet_lbl, buf);

}

lv_obj_t *ip_input_screen_create(void) {
    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    /* Title */
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "设置电脑 IP");
    lv_obj_set_style_text_color(t, WHITE, 0);
    lv_obj_set_style_text_font(t, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(t, 15, 20);

    /* Subtitle - current value */
    status_lbl = lv_label_create(scr);
    const char *cur = wifi_audio_get_pc_ip();
    char buf[64];
    snprintf(buf, sizeof(buf), "当前: %s", cur && cur[0] ? cur : "(无)");
    lv_label_set_text(status_lbl, buf);
    lv_obj_set_style_text_color(status_lbl, GREY, 0);
    lv_obj_set_style_text_font(status_lbl, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(status_lbl, 15, 50);

    /* Prefix label */
    prefix_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(prefix_lbl, WHITE, 0);
    lv_obj_set_style_text_font(prefix_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(prefix_lbl, 15, 90);

    /* Editable octet */
    octet_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(octet_lbl, ACCENT, 0);
    lv_obj_set_style_text_font(octet_lbl, &lv_font_montserrat_48, 0);
    lv_obj_align(octet_lbl, LV_ALIGN_CENTER, 0, -10);

    /* Hints */
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "UP/DOWN: 调整\nA: 确认  B: 取消");
    lv_obj_set_style_text_color(hint, SUB, 0);
    lv_obj_set_style_text_font(hint, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(hint, 15, 210);

    /* Init with last used or default 1 */
    int saved = wifi_audio_get_pc_last_octet();
    octet_val = saved > 0 ? saved : 1;
    update_display();

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    return scr;
}

void ip_input_navigate(int dx, int dy) {
    if (dy > 0) { octet_val++; if (octet_val > 254) octet_val = 254; }
    if (dy < 0) { octet_val--; if (octet_val < 1)   octet_val = 1; }
    update_display();
}

void ip_input_select(void) {
    wifi_audio_set_pc_last_octet(octet_val);
    wifi_audio_rebuild_pc_ip();

    char buf[64];
    snprintf(buf, sizeof(buf), "已保存: %s", wifi_audio_get_pc_ip());
    lv_label_set_text(status_lbl, buf);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0x4CAF50), 0);
}

void ip_input_cancel(void) {
    /* Just go back, no save */
}
