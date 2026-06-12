#include "rom_browser.h"
#include "app_manager.h"
#include "modules/nes/nes_wrapper.h"
#include "bsp/sd_card.h"
#include "bsp/st7789_driver.h"
#include "ui/display_driver.h"
#include "ui/components/status_bar.h"
#include "esp_log.h"
#include "lvgl.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static const char *TAG = "ROM_BROWSER";

#define MAX_ROMS 64
#define MAX_PATH 260

static char s_roms[MAX_ROMS][MAX_PATH];
static int s_rom_count = 0;
static int s_sel = 0;           /* 当前选中的ROM索引 */
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_btns[MAX_ROMS] = {0};

#define COLOR_NORMAL  0x0f3460
#define COLOR_SELECT  0xFF5C00

static void update_highlight(void) {
    for (int i = 0; i < s_rom_count; i++) {
        if (s_btns[i]) {
            lv_color_t c = lv_color_hex((i == s_sel) ? COLOR_SELECT : COLOR_NORMAL);
            lv_obj_set_style_bg_color(s_btns[i], c, 0);
        }
    }
    /* 滚动到选中项 */
    if (s_list && s_btns[s_sel]) {
        lv_obj_scroll_to_view(s_btns[s_sel], LV_ANIM_ON);
    }
}

static int scan_roms(void) {
    s_rom_count = 0;
    s_sel = 0;

    if (!sd_card_is_mounted()) {
        ESP_LOGI(TAG, "SD card not mounted, initializing...");
        ESP_LOGI(TAG, "DMA free before SD init: %zu",
                 heap_caps_get_free_size(MALLOC_CAP_DMA));
        esp_err_t ret = sd_card_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SD card init failed: %d", ret);
            return 0;
        }
    }

    const char *mp = sd_card_mount_point();
    ESP_LOGI(TAG, "SD mount point: %s, mounted=%d", mp, sd_card_is_mounted());

    char rom_dir[MAX_PATH];
    snprintf(rom_dir, sizeof(rom_dir), "%s/roms", mp);
    ESP_LOGI(TAG, "Looking for ROMs in: %s", rom_dir);

    DIR *dir = opendir(rom_dir);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open %s (errno=%d: %s)", rom_dir, errno, strerror(errno));
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_rom_count < MAX_ROMS) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 4 && (strcasecmp(name + len - 4, ".nes") == 0 ||
                        strcasecmp(name + len - 4, ".NES") == 0)) {
            size_t need = strlen(rom_dir) + 1 + len;
            if (need < MAX_PATH) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(s_roms[s_rom_count], MAX_PATH, "%s/%s", rom_dir, name);
#pragma GCC diagnostic pop
                ESP_LOGI(TAG, "Found ROM: %s", s_roms[s_rom_count]);
                s_rom_count++;
            }
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found %d NES ROM(s)", s_rom_count);
    return s_rom_count;
}

void rom_browser_enter(void) {
    int n = scan_roms();

    lv_obj_t *old = lv_scr_act();
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);

    /* Status bar */
    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    if (n > 0) {
        lv_label_set_text_fmt(title, "Select ROM (%d found)", n);
    } else {
        lv_label_set_text(title, "No ROMs found");
    }
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    if (n > 0) {
        s_list = lv_list_create(scr);
        lv_obj_set_size(s_list, 220, 200);
        lv_obj_align(s_list, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(s_list, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_border_width(s_list, 0, 0);

        for (int i = 0; i < n; i++) {
            const char *fname = strrchr(s_roms[i], '/');
            fname = fname ? fname + 1 : s_roms[i];

            lv_obj_t *btn = lv_list_add_btn(s_list, NULL, fname);
            lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_NORMAL), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
            s_btns[i] = btn;
        }
        /* 高亮第一项 */
        s_sel = 0;
        update_highlight();
    }

    lv_scr_load(scr);
    if (old) lv_obj_del(old);
}

/* 按键处理 — 由main.c的key_handler转发 */
void rom_browser_key(key_id_t key, bool pressed) {
    /* NES运行中：所有按键传给NES（按下+释放），B+START组合键退出由nes_task处理 */
    if (nes_is_running()) {
        nes_key_event(key, pressed);
        return;
    }

    /* ROM列表导航（只处理按下事件） */
    if (!pressed) return;
    ESP_LOGI(TAG, "Key: %d, sel=%d, count=%d", key, s_sel, s_rom_count);
    lvgl_lock();
    switch (key) {
        case KEY_UP:
            if (s_sel > 0) { s_sel--; update_highlight(); }
            break;
        case KEY_DOWN:
            if (s_sel < s_rom_count - 1) { s_sel++; update_highlight(); }
            break;
        case KEY_A:
            if (s_sel >= 0 && s_sel < s_rom_count) {
                ESP_LOGI(TAG, "=== LAUNCHING NES: %s ===", s_roms[s_sel]);
                nes_start(s_roms[s_sel]);
            }
            break;
        case KEY_B:
        case KEY_START:
            app_manager_return();
            break;
        default:
            break;
    }
    lvgl_unlock();
}
