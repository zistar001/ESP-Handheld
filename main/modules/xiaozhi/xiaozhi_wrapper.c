#include "xiaozhi_wrapper.h"
#include "esp_log.h"

static const char *TAG = "XIAOZHI";
static bool running = false;

esp_err_t xiaozhi_init(void) {
    ESP_LOGI(TAG, "XiaoZhi AI stub init");
    ESP_LOGW(TAG, "Full integration requires adapting audio_codec for ES8311+ES7210");
    ESP_LOGW(TAG, "Source at D:\\code\\XIAOZHI_AI\\xiaozhi-esp32-main\\main");
    return ESP_OK;
}

esp_err_t xiaozhi_start(void) {
    ESP_LOGI(TAG, "XiaoZhi AI start (stub)");
    running = true;
    return ESP_OK;
}

void xiaozhi_stop(void) {
    ESP_LOGI(TAG, "XiaoZhi AI stop (stub)");
    running = false;
}

bool xiaozhi_is_running(void) {
    return running;
}
