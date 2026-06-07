#include "ui/display_driver.h"
#include "bsp/st7789_driver.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "UI_DISP";

static bool nes_active = false;
static esp_lcd_panel_handle_t s_panel = NULL;

void ui_display_set_nes_active(bool active)
{
    nes_active = active;
    /* NES退出时，标记整个屏幕为脏，触发LVGL全屏重绘 */
    if (!active) {
        lv_obj_t *scr = lv_scr_act();
        if (scr) {
            lv_obj_invalidate(scr);
        }
    }
}

/* LVGL display flush callback: writes pixels to ST7789 via ESP-IDF panel API */
static void disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (nes_active) {
        lv_disp_flush_ready(drv);
        return;
    }

    int32_t x1 = area->x1, y1 = area->y1;
    int32_t x2 = area->x2, y2 = area->y2;

    /* 每次获取最新panel句柄（LCD可能被重新初始化过） */
    esp_lcd_panel_handle_t p = st7789_get_panel();
    if (p) {
        st7789_spi_lock();
        esp_lcd_panel_draw_bitmap(p, x1, y1, x2 + 1, y2 + 1, color_p);
        st7789_spi_unlock();
    }

    lv_disp_flush_ready(drv);
}

/* LVGL tick task: feeds LVGL time reference and processes timers */
static void lvgl_tick_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
        if (!nes_active) {
            lv_timer_handler();
        }
    }
}

esp_err_t ui_display_init(void)
{
    lv_init();
    static lv_color_t *buf1 = NULL;
    static lv_color_t *buf2 = NULL;
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t disp_drv;

    /* Get panel handle from ST7789 driver */
    s_panel = st7789_get_panel();
    if (!s_panel) {
        ESP_LOGE(TAG, "ST7789 panel not initialized!");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate double display buffers (80 lines each) for smoother rendering */
    size_t buf_size = ST7789_WIDTH * 80 * sizeof(lv_color_t);
    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1) {
        buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_8BIT);
    }
    buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf2) {
        buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_8BIT);
    }
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate display buffers (%zu bytes each)", buf_size);
        if (buf1) free(buf1);
        if (buf2) free(buf2);
        return ESP_ERR_NO_MEM;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, ST7789_WIDTH * 80);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ST7789_WIDTH;
    disp_drv.ver_res = ST7789_HEIGHT;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Create LVGL tick task on Core 0 */
    xTaskCreatePinnedToCore(lvgl_tick_task, "lv_tick", 4096, NULL, 1, NULL, 0);

    ESP_LOGI(TAG, "Display initialized (%dx%d, double-buffered)", ST7789_WIDTH, ST7789_HEIGHT);
    return ESP_OK;
}
