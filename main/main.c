#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Handheld starting...");
    ESP_LOGI(TAG, "Build verification - toolchain OK");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
