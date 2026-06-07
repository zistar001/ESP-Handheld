#include "weather.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "ui/screens/home_screen.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEATHER";

/* HeFeng API config — edit to match your console credentials */
#define HEFENG_API_KEY   "700cf8ab08774bf089e52d33b89aecf8"
#define HEFENG_LOCATION  "101230501"  /* default: Quanzhou (泉州) city ID */
#define WEATHER_INTERVAL_SEC 1800     /* 30 min between fetches */
#define WIFI_TIMEOUT_MS   30000

static TaskHandle_t s_weather_task = NULL;

/* ── HTTP response collector ── */
static char *s_resp = NULL;
static size_t s_resp_len = 0;

static esp_err_t http_evt_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            char *p = realloc(s_resp, s_resp_len + evt->data_len + 1);
            if (!p) return ESP_ERR_NO_MEM;
            s_resp = p;
            memcpy(s_resp + s_resp_len, evt->data, evt->data_len);
            s_resp_len += evt->data_len;
            s_resp[s_resp_len] = '\0';
            break;
        }
        case HTTP_EVENT_ON_FINISH: {
            if (!s_resp) break;
            cJSON *root = cJSON_Parse(s_resp);
            if (!root) {
                ESP_LOGW(TAG, "JSON error: %.120s", s_resp);
                break;
            }
            cJSON *code = cJSON_GetObjectItem(root, "code");
            const char *code_str = cJSON_IsString(code) ? code->valuestring : NULL;
            if (!code_str || strcmp(code_str, "200") != 0) {
                ESP_LOGW(TAG, "API error: %s",
                         code_str ? code_str : "null/not-string");
                cJSON_Delete(root);
                break;
            }
            cJSON *now = cJSON_GetObjectItem(root, "now");
            if (now) {
                cJSON *text = cJSON_GetObjectItem(now, "text");
                cJSON *temp = cJSON_GetObjectItem(now, "temp");
                if (text && temp) {
                    ESP_LOGI(TAG, "Weather: %s  %s°C",
                             text->valuestring, temp->valuestring);
                    home_screen_update_weather("", text->valuestring,
                                               atoi(temp->valuestring),
                                               0, 0, text->valuestring);
                }
            }
            cJSON_Delete(root);
            break;
        }
        case HTTP_EVENT_DISCONNECTED:
            free(s_resp);
            s_resp = NULL;
            s_resp_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t weather_fetch_now(void) {
    char url[256];
    snprintf(url, sizeof(url),
             "https://devapi.qweather.com/v7/weather/now?location=%s&key=%s",
             HEFENG_LOCATION, HEFENG_API_KEY);

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_evt_handler,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t cl = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(cl);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "HTTP failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(cl);
    return err;
}

/* ── Background task ── */
static void weather_task(void *arg) {
    wifi_manager_init();

    while (1) {
        if (wifi_manager_wait_connected(pdMS_TO_TICKS(WIFI_TIMEOUT_MS)))
            weather_fetch_now();
        else
            ESP_LOGW(TAG, "No WiFi, skipping weather");
        vTaskDelay(pdMS_TO_TICKS(WEATHER_INTERVAL_SEC * 1000));
    }
}

/* ── Public API ── */
esp_err_t weather_init(void) {
    if (s_weather_task) return ESP_OK;
    xTaskCreatePinnedToCore(weather_task, "weather", 4096, NULL, 2,
                            &s_weather_task, 0);
    ESP_LOGI(TAG, "Weather module started");
    return ESP_OK;
}

void weather_set_api_key(const char *key) { (void)key; }
void weather_set_location(const char *loc) { (void)loc; }
