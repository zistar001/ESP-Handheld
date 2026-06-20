#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THEME_COUNT 6

typedef enum {
    THEME_CLASSIC_ORANGE = 0,  /* 经典橙 */
    THEME_AURORA_BLUE,         /* 极光蓝 */
    THEME_CYBER_PURPLE,        /* 赛博紫 */
    THEME_JADE_GREEN,          /* 翡翠绿 */
    THEME_DARK_GOLD,           /* 暗紫金 */
    THEME_STEEL_GREY,          /* 钢铁灰 */
} theme_id_t;

typedef enum {
    TSLOT_BG = 0,
    TSLOT_ACCENT,
    TSLOT_HIGHLIGHT,
    TSLOT_TEXT,
    TSLOT_SUBTEXT,
    TSLOT_COUNT,  /* = 5 */
} theme_color_slot_t;

/* 单套主题的定义 */
typedef struct {
    const char  *name;         /* 中文名称 */
    uint32_t     colors[TSLOT_COUNT];  /* 5个颜色槽 */
} theme_def_t;

/* 用户配置：基础主题 + 逐槽覆盖 */
typedef struct {
    theme_id_t  base_theme;
    uint8_t     override_mask;          /* bit0-4 对应 5个槽 */
    uint32_t    override_colors[TSLOT_COUNT];
} theme_config_t;

/* ─── API ─── */

/* 获取当前主题某个颜色槽的值（考虑覆盖） */
uint32_t    theme_get_color(theme_color_slot_t slot);

/* 获取某个主题的某个槽原始值 */
uint32_t    theme_get_base_color(theme_id_t theme, theme_color_slot_t slot);
const char *theme_get_name(theme_id_t id);

/* 加载/保存 NVS */
void        theme_load(void);
void        theme_save(void);

/* 设置基础主题（清除所有覆盖） */
void        theme_set_base(theme_id_t id);
theme_id_t  theme_get_base(void);

/* 覆盖/清除单个颜色槽 */
void        theme_set_override(theme_color_slot_t slot, uint32_t color);
void        theme_clear_override(theme_color_slot_t slot);
bool        theme_has_override(theme_color_slot_t slot);
uint32_t    theme_get_override(theme_color_slot_t slot);

/* 获取当前完整配置（只读） */
const theme_config_t *theme_get_config(void);

/* 获取所有主题定义表 */
const theme_def_t *theme_get_defs(void);

#ifdef __cplusplus
}
#endif
