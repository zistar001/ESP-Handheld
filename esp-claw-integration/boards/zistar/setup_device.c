/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zistar ESP-Handheld — board-specific device factories.
 * Provides the ST7789 panel factory with correct offset for 240x280 visible area.
 */
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "ZISTAR_SETUP";

/* ST7789 native resolution is 240x320; visible portion starts at row 20 */
#define ZISTAR_LCD_OFFSET_X  0
#define ZISTAR_LCD_OFFSET_Y  20

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    esp_lcd_panel_dev_config_t panel_dev_cfg = {0};
    memcpy(&panel_dev_cfg, panel_dev_config, sizeof(esp_lcd_panel_dev_config_t));

    esp_err_t ret = esp_lcd_new_panel_st7789(io, &panel_dev_cfg, ret_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7789 failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Apply panel offset for the 240x280 visible window */
    ret = esp_lcd_panel_set_gap(*ret_panel, ZISTAR_LCD_OFFSET_X, ZISTAR_LCD_OFFSET_Y);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_lcd_panel_set_gap failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Zistar LCD init complete (240x280, gap=%d,%d)",
             ZISTAR_LCD_OFFSET_X, ZISTAR_LCD_OFFSET_Y);
    return ESP_OK;
}
