#pragma once

/* ================================================================
 * 主题颜色便捷宏 — 在各屏幕文件中代替硬编码颜色
 *
 * 用法: 在屏幕 .c 文件中 #include 此文件，
 *       然后用 CLR_BG / CLR_ACCENT 等代替原来的硬编码颜色。
 * ================================================================ */

#include "modules/theme/theme_manager.h"

/* 当前主题的 5 个颜色槽（每次取值实时查询，支持运行时切换） */
#define CLR_BG        lv_color_hex(theme_get_color(TSLOT_BG))
#define CLR_ACCENT    lv_color_hex(theme_get_color(TSLOT_ACCENT))
#define CLR_HIGHLIGHT lv_color_hex(theme_get_color(TSLOT_HIGHLIGHT))
#define CLR_TEXT      lv_color_hex(theme_get_color(TSLOT_TEXT))
#define CLR_SUBTEXT   lv_color_hex(theme_get_color(TSLOT_SUBTEXT))
