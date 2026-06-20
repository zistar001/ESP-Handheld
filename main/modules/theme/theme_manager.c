#include "theme_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "THEME";
#define NVS_NS "theme"

/* ================================================================
 * 6套主题定义
 * ================================================================ */
static const theme_def_t s_themes[THEME_COUNT] = {
    /*                     BG         ACCENT     HIGHLIGHT  TEXT      SUBTEXT */
    [THEME_CLASSIC_ORANGE] = {"经典橙", {0x0A0A0A, 0xFF5C00, 0xFFBB00, 0xFFFFFF, 0x999999}},
    [THEME_AURORA_BLUE]    = {"极光蓝", {0x0D1117, 0x0096FF, 0x58A6FF, 0xFFFFFF, 0x8B949E}},
    [THEME_CYBER_PURPLE]   = {"赛博紫", {0x0A0015, 0xBB86FC, 0x00FFFF, 0xFFFFFF, 0x8870A0}},
    [THEME_JADE_GREEN]     = {"翡翠绿", {0x0A0F0A, 0x00C853, 0x69F0AE, 0xFFFFFF, 0xA5D6A7}},
    [THEME_DARK_GOLD]      = {"暗紫金", {0x120A1A, 0xBB86FC, 0xFFD700, 0xFFFFFF, 0x9980B0}},
    [THEME_STEEL_GREY]     = {"钢铁灰", {0x121212, 0xFF6D00, 0xFF9100, 0xFFFFFF, 0xB0B0B0}},
};

/* ── 当前运行时配置 ── */
static theme_config_t s_cfg = {
    .base_theme = THEME_CLASSIC_ORANGE,
    .override_mask = 0,
};

const theme_def_t *theme_get_defs(void) { return s_themes; }

const char *theme_get_name(theme_id_t id) {
    if (id < 0 || id >= THEME_COUNT) return "?";
    return s_themes[id].name;
}

uint32_t theme_get_base_color(theme_id_t theme, theme_color_slot_t slot) {
    if (theme < 0 || theme >= THEME_COUNT) return 0;
    if (slot < 0 || slot >= TSLOT_COUNT) return 0;
    return s_themes[theme].colors[slot];
}

/* ── 核心：获取最终颜色（考虑覆盖） ── */
uint32_t theme_get_color(theme_color_slot_t slot) {
    if (slot < 0 || slot >= TSLOT_COUNT) return 0;
    if (s_cfg.override_mask & (1 << slot))
        return s_cfg.override_colors[slot];
    return s_themes[s_cfg.base_theme].colors[slot];
}

/* ── 读写配置 ── */
theme_id_t theme_get_base(void) { return s_cfg.base_theme; }

void theme_set_base(theme_id_t id) {
    if (id < 0 || id >= THEME_COUNT) return;
    s_cfg.base_theme = id;
    s_cfg.override_mask = 0;  /* 切换基础主题时清除所有覆盖 */
    ESP_LOGI(TAG, "Theme set to %s", s_themes[id].name);
    theme_save();
}

void theme_set_override(theme_color_slot_t slot, uint32_t color) {
    if (slot < 0 || slot >= TSLOT_COUNT) return;
    s_cfg.override_mask |= (1 << slot);
    s_cfg.override_colors[slot] = color;
    theme_save();
}

void theme_clear_override(theme_color_slot_t slot) {
    if (slot < 0 || slot >= TSLOT_COUNT) return;
    s_cfg.override_mask &= ~(1 << slot);
    theme_save();
}

bool theme_has_override(theme_color_slot_t slot) {
    return (s_cfg.override_mask & (1 << slot)) != 0;
}

uint32_t theme_get_override(theme_color_slot_t slot) {
    if (slot < 0 || slot >= TSLOT_COUNT) return 0;
    return s_cfg.override_colors[slot];
}

const theme_config_t *theme_get_config(void) { return &s_cfg; }

/* ── NVS ── */
void theme_load(void) {
    nvs_handle_t h;
    esp_err_t e = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "No saved theme (will use default)");
        return;
    }
    size_t sz = sizeof(s_cfg);
    e = nvs_get_blob(h, "data", &s_cfg, &sz);
    nvs_close(h);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load theme config, using default");
        s_cfg.base_theme = THEME_CLASSIC_ORANGE;
        s_cfg.override_mask = 0;
    } else {
        ESP_LOGI(TAG, "Theme loaded: base=%s, mask=0x%02x",
                 s_themes[s_cfg.base_theme].name, s_cfg.override_mask);
    }
}

void theme_save(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "data", &s_cfg, sizeof(s_cfg));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Theme saved");
}
