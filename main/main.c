#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "bsp_board.h"
#include "st7789_driver.h"

static const char *TAG = "MAIN";

static void show_test_pattern(void) {
    size_t buf_size = ST7789_WIDTH * ST7789_HEIGHT * 2;
    uint8_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!buf) { ESP_LOGE(TAG, "No PSRAM"); return; }
    for (int y = 0; y < ST7789_HEIGHT; y++)
        for (int x = 0; x < ST7789_WIDTH; x++) {
            int i = (y * ST7789_WIDTH + x) * 2;
            uint16_t c = (x < 80) ? 0xF800 : (x < 160) ? 0x07E0 : 0x001F;
            buf[i] = c >> 8; buf[i+1] = c & 0xFF;
        }
    st7789_set_window(0, 0, ST7789_WIDTH-1, ST7789_HEIGHT-1);
    st7789_write_ram(buf, buf_size);
    free(buf);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting...");
    bsp_board_init();
    show_test_pattern();
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
