#include "ui/display_driver.h"
#include "bsp/st7789_driver.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"

static const char *TAG = "UI_DISP";

static bool nes_active = false;
static SemaphoreHandle_t s_lvgl_mutex = NULL;

void lvgl_lock(void) {
    if (s_lvgl_mutex) xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
}

void lvgl_unlock(void) {
    if (s_lvgl_mutex) xSemaphoreGive(s_lvgl_mutex);
}

void ui_display_set_nes_active(bool active) {
    nes_active = active;
    if (!active) {
        lvgl_lock();
        lv_obj_t *scr = lv_scr_act();
        if (scr) lv_obj_invalidate(scr);
        lvgl_unlock();
    }
}

/* LVGL flush callback */
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (nes_active) { lv_disp_flush_ready(drv); return; }

    esp_lcd_panel_handle_t p = st7789_get_panel();
    if (p) {
        st7789_spi_lock();
        esp_lcd_panel_draw_bitmap(p, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
        st7789_spi_unlock();
    }
    lv_disp_flush_ready(drv);
}

/* LVGL tick task */
static void lvgl_tick_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
        if (!nes_active) {
            lvgl_lock();
            lv_timer_handler();
            lvgl_unlock();
        }
    }
}

esp_err_t ui_display_init(void) {
    if (!s_lvgl_mutex) s_lvgl_mutex = xSemaphoreCreateMutex();
    lv_init();

    esp_lcd_panel_handle_t p = st7789_get_panel();
    if (!p) { ESP_LOGE(TAG, "ST7789 panel not ready"); return ESP_ERR_INVALID_STATE; }

    /* XiaoZhi-style: 画白色全屏开机画面 */
    ESP_LOGI(TAG, "Drawing white boot screen...");
    size_t buf_sz = ST7789_WIDTH * sizeof(uint16_t);
    uint16_t *white = (uint16_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_DMA);
    if (white) {
        for (int i = 0; i < ST7789_WIDTH; i++) white[i] = 0xFFFF;
        for (int y = 0; y < ST7789_HEIGHT; y++) {
            esp_lcd_panel_draw_bitmap(p, 0, y, ST7789_WIDTH, y + 1, white);
        }
        free(white);
    }

    /* LVGL display buffer — 双缓冲解决DMA未完成时LVGL重写数据导致的条纹 */
    size_t buf_size = ST7789_WIDTH * 20 * sizeof(lv_color_t);
    lv_color_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    lv_color_t *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!buf) {
        buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        buf2 = NULL;
    }
    if (!buf2 && buf) {
        buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!buf) {
        buf = heap_caps_malloc(buf_size, MALLOC_CAP_8BIT);
        buf2 = NULL;
    }
    if (!buf) { ESP_LOGE(TAG, "Buffer alloc failed"); return ESP_ERR_NO_MEM; }

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, buf2, ST7789_WIDTH * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ST7789_WIDTH;
    disp_drv.ver_res = ST7789_HEIGHT;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    /* offset handled by esp_lcd_panel_set_gap(0,20) in st7789_init */
    disp_drv.offset_x = 0;
    disp_drv.offset_y = 0;
    lv_disp_drv_register(&disp_drv);

    xTaskCreatePinnedToCore(lvgl_tick_task, "lv_tick", 4096, NULL, 1, NULL, 0);
    ESP_LOGI(TAG, "Display ready (XiaoZhi-compatible)");
    return ESP_OK;
}
