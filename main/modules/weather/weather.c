#include "weather.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "app/app_manager.h"
#include "ui/screens/home_screen.h"
#include "ui/display_driver.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "zlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEATHER";
#define HEFENG_API_KEY   "700cf8ab08774bf089e52d33b89aecf8"
#define HEFENG_LOCATION  "101230501"
#define WEATHER_INTERVAL_SEC 1800
#define WIFI_TIMEOUT_MS   30000
#define HEFENG_HOST      "p23p3qvugk.re.qweatherapi.com"
static TaskHandle_t s_weather_task = NULL;
static int s_today_high = 0, s_today_low = 0;  /* cached from 3d response */

/* ── Gzip decompression via zlib inflate ── */
static char *gunzip(const void *src, size_t src_len) {
    const unsigned char *s = (const unsigned char *)src;
    if (src_len < 18 || s[0] != 0x1F || s[1] != 0x8B || s[2] != 8)
        return NULL;

    /* Use inflate in a loop with growing buffer to handle any response size */
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = (void *)s;
    strm.avail_in = src_len;
    int ret = inflateInit2(&strm, 16 + MAX_WBITS);
    if (ret != Z_OK) return NULL;

    size_t alloc = 4096;
    char *dst = malloc(alloc);
    if (!dst) { inflateEnd(&strm); return NULL; }

    do {
        strm.next_out = (unsigned char *)dst + strm.total_out;
        strm.avail_out = alloc - strm.total_out;
        if (strm.avail_out < 1024) {
            /* Double the buffer */
            alloc *= 2;
            char *p = realloc(dst, alloc);
            if (!p) { free(dst); inflateEnd(&strm); return NULL; }
            dst = p;
            strm.next_out = (unsigned char *)dst + strm.total_out;
            strm.avail_out = alloc - strm.total_out;
        }
        ret = inflate(&strm, Z_NO_FLUSH);
    } while (ret == Z_OK);

    if (ret != Z_STREAM_END) {
        inflateEnd(&strm); free(dst); return NULL;
    }
    inflateEnd(&strm);

    dst[strm.total_out] = '\0';
    return dst;
}

static void parse_weather(const char *body) {
    cJSON *root = cJSON_Parse(body);
    if (!root) { ESP_LOGW(TAG, "JSON"); return; }
    cJSON *code = cJSON_GetObjectItem(root, "code");
    const char *cs = cJSON_IsString(code) ? code->valuestring : NULL;
    if (!cs || strcmp(cs, "200")) { cJSON_Delete(root); return; }
    /* Extract location name (priority: adm2 district > name city) */
    const char *city = "\xe6\xb3\x89\xe5\xb7\x9e"; /* fallback: 泉州 */
    cJSON *loc = cJSON_GetObjectItem(root, "location");
    if (loc) {
        cJSON *adm2 = cJSON_GetObjectItem(loc, "adm2");
        cJSON *name = cJSON_GetObjectItem(loc, "name");
        if (cJSON_IsString(adm2) && adm2->valuestring[0])
            city = adm2->valuestring;               /* district/county */
        else if (cJSON_IsString(name) && name->valuestring[0])
            city = name->valuestring;
    }

    cJSON *now = cJSON_GetObjectItem(root, "now");
    if (now) {
        cJSON *text = cJSON_GetObjectItem(now, "text");
        cJSON *temp = cJSON_GetObjectItem(now, "temp");
        if (text && temp) {
            ESP_LOGI(TAG, "Weather: %s %d~%dC %s", city, s_today_low, s_today_high, text->valuestring);
            lvgl_lock();
            if (app_manager_get_state() == APP_STATE_LAUNCHER)
                home_screen_update_weather(city, text->valuestring, atoi(temp->valuestring), s_today_high, s_today_low, NULL);
            lvgl_unlock();
        }
    }
    cJSON *d = cJSON_GetObjectItem(root, "daily");
    if (d && cJSON_IsArray(d) && app_manager_get_state() == APP_STATE_LAUNCHER) {
        int n = cJSON_GetArraySize(d); if(n>4)n=4;
        lvgl_lock();
        /* Update today's high/low from daily[0] */
        if (n > 0) {
            cJSON *d0 = cJSON_GetArrayItem(d, 0);
            if (d0) {
                cJSON *dhi = cJSON_GetObjectItem(d0, "tempMax");
                cJSON *dlo = cJSON_GetObjectItem(d0, "tempMin");
                if (dhi) s_today_high = atoi(dhi->valuestring);
                if (dlo) s_today_low  = atoi(dlo->valuestring);
                /* Update range only (temp unchanged) */
                home_screen_update_weather(NULL, NULL, -999, s_today_high, s_today_low, NULL);
            }
        }
        for(int i=1;i<n&&i<4;i++){  /* skip today, show 3 future days */
            cJSON *day=cJSON_GetArrayItem(d,i); if(!day)continue;
            cJSON *fxDate=cJSON_GetObjectItem(day,"fxDate");
            cJSON *dmax=cJSON_GetObjectItem(day,"tempMax");
            cJSON *dmin=cJSON_GetObjectItem(day,"tempMin");
            cJSON *dtext=cJSON_GetObjectItem(day,"textDay");
            const char *date_str = fxDate ? fxDate->valuestring : NULL;
            home_screen_update_forecast(i-1, date_str,
                dmax?atoi(dmax->valuestring):0, dmin?atoi(dmin->valuestring):0,
                dtext ? dtext->valuestring : NULL);
        }
        lvgl_unlock();
    }
    cJSON_Delete(root);
}

static char *s_resp = NULL;
static size_t s_resp_len = 0;
static void reset_resp(void) { free(s_resp); s_resp = NULL; s_resp_len = 0; }

static esp_err_t evt(esp_http_client_event_t *e) {
    switch (e->event_id) {
        case HTTP_EVENT_ON_DATA: {
            char *p = realloc(s_resp, s_resp_len + e->data_len + 1);
            if (!p) return ESP_ERR_NO_MEM;
            s_resp = p;
            memcpy(s_resp + s_resp_len, e->data, e->data_len);
            s_resp_len += e->data_len;
            s_resp[s_resp_len] = 0;
            break;
        }
        case HTTP_EVENT_ON_FINISH: {
            if (!s_resp || !s_resp_len) break;
            char *body = NULL;
            if (s_resp_len >= 2 && (unsigned char)s_resp[0]==0x1F && (unsigned char)s_resp[1]==0x8B) {
                /* Try gzip decompression with larger output buffer */
                body = gunzip(s_resp, s_resp_len);
                if (!body) {
                    ESP_LOGW(TAG, "gunzip fail len=%u", (unsigned)s_resp_len);
                    break;
                }
            } else {
                body = strdup(s_resp);
            }
            if (body) { parse_weather(body); free(body); }
            break;
        }
        case HTTP_EVENT_DISCONNECTED: reset_resp(); break;
        default: break;
    }
    return ESP_OK;
}

static esp_err_t wf(const char *api_path) {
    char path[320];
    snprintf(path, sizeof(path), "%s?location=%s&key=%s&gzip=n", api_path, HEFENG_LOCATION, HEFENG_API_KEY);
    esp_http_client_config_t cfg = {
        .host = HEFENG_HOST, .path = path, .port = 443,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = evt, .timeout_ms = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t cl = esp_http_client_init(&cfg);
    if (!cl) return ESP_ERR_NO_MEM;
    reset_resp();
    esp_err_t err = esp_http_client_perform(cl);
    if (err) ESP_LOGE(TAG, "HTTP: %s", esp_err_to_name(err));
    esp_http_client_cleanup(cl);
    s_resp = NULL; s_resp_len = 0;
    return err;
}

static void weather_task(void *arg) {
    wifi_manager_init();
    while (1) {
        if (wifi_manager_wait_connected(pdMS_TO_TICKS(WIFI_TIMEOUT_MS))) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            for (int r=0;r<3;r++){ if(wf("/v7/weather/now")==0)break; vTaskDelay(pdMS_TO_TICKS(5000)); }
            vTaskDelay(pdMS_TO_TICKS(2000));
            for (int r=0;r<3;r++){ if(wf("/v7/weather/7d")==0)break; vTaskDelay(pdMS_TO_TICKS(5000)); }
        }
        vTaskDelay(pdMS_TO_TICKS(WEATHER_INTERVAL_SEC*1000));
    }
}

esp_err_t weather_init(void) {
    if (s_weather_task) return ESP_OK;
    xTaskCreatePinnedToCore(weather_task,"weather",4096,NULL,2,&s_weather_task,0);
    ESP_LOGI(TAG,"started"); return ESP_OK;
}
void weather_set_api_key(const char *k){(void)k;}
void weather_set_location(const char *l){(void)l;}
