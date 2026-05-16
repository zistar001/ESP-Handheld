#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "bsp_board.h"
#include "st7789_driver.h"
#include "modules/sensor/aht20_driver.h"
#include "modules/imu/imu_driver.h"

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

static void sensor_task(void *arg) {
    aht20_init();
    while (1) {
        float t, h;
        if (aht20_read(&t, &h) == ESP_OK)
            ESP_LOGI(TAG, "AHT20: %.1f°C %.1f%%", t, h);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void imu_task(void *arg) {
    if (imu_init() != ESP_OK) {
        ESP_LOGE(TAG, "IMU init failed, removing task");
        vTaskDelete(NULL);
    }
    while (1) {
        imu_data_t d;
        if (imu_read(&d) == ESP_OK)
            ESP_LOGI(TAG, "IMU: ax=%.2f ay=%.2f az=%.2f gx=%.1f gy=%.1f gz=%.1f",
                     d.ax, d.ay, d.az, d.gx, d.gy, d.gz);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting...");
    bsp_board_init();
    show_test_pattern();

    xTaskCreatePinnedToCore(sensor_task, "sensor", 3072, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(imu_task, "imu", 3072, NULL, 3, NULL, 0);

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
