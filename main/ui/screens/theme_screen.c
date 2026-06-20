#include "theme_screen.h"
#include "ui/components/status_bar.h"
#include "ui/components/theme_colors.h"
#include "modules/theme/theme_manager.h"
#include <stdio.h>
#include <string.h>

#define BG       lv_color_hex(0x0A0A0A)
#define WHITE    lv_color_hex(0xFFFFFF)
#define ORANGE   lv_color_hex(0xFF5C00)
#define GREY     lv_color_hex(0x333333)
#define SUB      lv_color_hex(0x999999)

/* 6 基础主题 + 5 颜色槽覆盖 = 11 行 */
#define BASE_COUNT 6
#define SLOT_COUNT 5
#define TOTAL_ITEMS (BASE_COUNT + SLOT_COUNT)

static const char *s_slot_names[SLOT_COUNT] = {
    "背景色", "主色", "高亮色", "文字色", "副文色",
};

static lv_obj_t *s_rows[TOTAL_ITEMS];
static lv_obj_t *s_scr = NULL;
static int s_sel = 0;

/* ── 查找某个当前颜色来自哪个主题（用于显示来源） ── */
static int find_slot_source(theme_color_slot_t slot) {
    uint32_t cur = theme_get_color(slot);
    for (int i = 0; i < THEME_COUNT; i++) {
        if (theme_get_base_color((theme_id_t)i, slot) == cur)
            return i;
    }
    return -1; /* 自定义 */
}

/* ── 刷新所有行 ── */
static void refresh_ui(void) {
    for (int i = 0; i < TOTAL_ITEMS; i++) {
        if (!s_rows[i]) continue;

        lv_obj_t *lbl = lv_obj_get_child(s_rows[i], 0);
        if (!lbl) continue;

        char buf[64];
        if (i < BASE_COUNT) {
            /* 基础主题行: 显示 ○/● + 主题名 */
            bool active = (theme_get_base() == (theme_id_t)i);
            snprintf(buf, sizeof(buf), "%s %s",
                     active ? "\xE2\x97\x8F" : "\xE2\x97\x8B",
                     theme_get_name((theme_id_t)i));
            lv_label_set_text(lbl, buf);
            lv_obj_set_style_text_color(lbl, active ? ORANGE : WHITE, 0);
        } else {
            /* 颜色槽行: 显示 "色名: 来源主题" */
            int slot = i - BASE_COUNT;
            int src = find_slot_source((theme_color_slot_t)slot);
            const char *src_name = (src >= 0) ? theme_get_name((theme_id_t)src) : "自定义";

            /* 如果这个槽有覆盖标记，加个 * 前缀 */
            if (theme_has_override((theme_color_slot_t)slot))
                snprintf(buf, sizeof(buf), "%s: %s *", s_slot_names[slot], src_name);
            else
                snprintf(buf, sizeof(buf), "%s: %s", s_slot_names[slot], src_name);

            lv_label_set_text(lbl, buf);
            lv_obj_set_style_text_color(lbl, WHITE, 0);
        }

        /* 高亮当前选中行 */
        lv_obj_set_style_bg_color(s_rows[i], (i == s_sel) ? ORANGE : GREY, 0);
    }
}

/* ── 选中/切换 ── */
void theme_screen_select(void) {
    if (s_sel < BASE_COUNT) {
        /* 选择基础主题 → 清除所有覆盖 */
        theme_set_base((theme_id_t)s_sel);
    } else {
        /* 循环颜色槽: 找下一个主题的同槽颜色 */
        int slot = s_sel - BASE_COUNT;
        int cur_src = find_slot_source((theme_color_slot_t)slot);
        int next = (cur_src + 1) % THEME_COUNT;
        uint32_t new_color = theme_get_base_color((theme_id_t)next, (theme_color_slot_t)slot);

        if (new_color == theme_get_base_color(theme_get_base(), (theme_color_slot_t)slot)) {
            /* 如果跟基础主题一样，清除覆盖 */
            theme_clear_override((theme_color_slot_t)slot);
        } else {
            theme_set_override((theme_color_slot_t)slot, new_color);
        }
    }
    refresh_ui();
}

void theme_screen_navigate(int dir) {
    int old = s_sel;
    s_sel += dir;
    if (s_sel < 0) s_sel = 0;
    if (s_sel >= TOTAL_ITEMS) s_sel = TOTAL_ITEMS - 1;
    if (s_sel != old) refresh_ui();
}

/* ── 创建界面 ── */
lv_obj_t *theme_screen_create(void) {
    s_sel = 0;
    lv_obj_t *old = lv_scr_act();
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, BG, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    status_bar_create(s_scr);

    /* 标题 */
    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, "主题");
    lv_obj_set_style_text_color(title, WHITE, 0);
    lv_obj_set_style_text_font(title, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(title, 15, 36);

    /* 滚动容器 */
    lv_obj_t *cont = lv_obj_create(s_scr);
    lv_obj_set_size(cont, 240, 220);
    lv_obj_set_pos(cont, 0, 58);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);

    /* 6 行基础主题 + 分隔提示 + 5 行颜色槽 */
    int idx = 0;

    /* 基础主题区域 */
    for (int i = 0; i < BASE_COUNT; i++) {
        s_rows[idx] = lv_btn_create(cont);
        lv_obj_set_size(s_rows[idx], 220, 28);
        lv_obj_set_pos(s_rows[idx], 10, idx * 30);
        lv_obj_set_style_radius(s_rows[idx], 6, 0);
        lv_obj_set_style_border_width(s_rows[idx], 0, 0);
        lv_obj_set_style_pad_all(s_rows[idx], 0, 0);
        lv_obj_t *l = lv_label_create(s_rows[idx]);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(l, 10, 0);
        idx++;
    }

    /* 分隔标签 */
    lv_obj_t *sep = lv_label_create(cont);
    lv_label_set_text(sep, "── 自定义混搭 ──");
    lv_obj_set_style_text_color(sep, SUB, 0);
    lv_obj_set_style_text_font(sep, &lv_font_simsun_16_cjk, 0);
    lv_obj_set_pos(sep, 50, idx * 30 + 5);

    idx = BASE_COUNT;

    /* 颜色槽区域 */
    for (int i = 0; i < SLOT_COUNT; i++) {
        s_rows[idx] = lv_btn_create(cont);
        lv_obj_set_size(s_rows[idx], 220, 28);
        lv_obj_set_pos(s_rows[idx], 10, idx * 30 + 25);
        lv_obj_set_style_radius(s_rows[idx], 6, 0);
        lv_obj_set_style_border_width(s_rows[idx], 0, 0);
        lv_obj_set_style_pad_all(s_rows[idx], 0, 0);
        lv_obj_t *l = lv_label_create(s_rows[idx]);
        lv_obj_set_style_text_font(l, &lv_font_simsun_16_cjk, 0);
        lv_obj_set_pos(l, 10, 0);
        idx++;
    }

    /* 底部提示 */
    lv_obj_t *hint = lv_label_create(s_scr);
    lv_label_set_text(hint, "A=选择/切换  B=返回");
    lv_obj_set_style_text_color(hint, SUB, 0);
    lv_obj_set_style_text_font(hint, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -5);

    refresh_ui();

    lv_scr_load(s_scr);
    if (old) lv_obj_del(old);
    return s_scr;
}
