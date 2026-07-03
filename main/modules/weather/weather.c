#include "weather.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "ui/screens/home_screen.h"
#include "ui/display_driver.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "nvs.h"

/* 让cJSON使用PSRAM，避免内部SRAM不足 */
static void *cjson_malloc(size_t sz) { return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void cjson_free(void *p) { heap_caps_free(p); }
#include "zlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEATHER";
#define DEFAULT_LOCATION "101230501"      /* 泉州本级，无 NVS 时的后备 */
#define WEATHER_INTERVAL_SEC 1800
#define WIFI_TIMEOUT_MS   30000
#define HEFENG_HOST      "devapi.qweather.com"      /* 和风天气 API — 可在 menuconfig 中修改 */
#define NVS_NS           "weather"

/* API 密钥从 NVS 动态加载，不再硬编码 */
static char s_api_key[48] = "";
static bool s_skip_weather = true;  /* 无密钥时跳过天气请求 */

static TaskHandle_t s_weather_task = NULL;
static int s_today_high = 0, s_today_low = 0;  /* cached from 3d response */

/* ── 地区映射 ── */
static const weather_loc_entry_t s_locations[] = {
    { "101230501", "\xe9\xb2\xa4\xe5\x9f\x8e\xe5\x8c\xba" },              /* 鲤城区 */
    { "101230501", "\xe4\xb8\xb0\xe6\xb3\xbd\xe5\x8c\xba" },              /* 丰泽区 */
    { "101230501", "\xe6\xb4\x9b\xe6\xb1\x9f\xe5\x8c\xba" },              /* 洛江区 */
    { "101230515", "\xe6\xb3\x89\xe6\xb8\xaf\xe5\x8c\xba" },              /* 泉港区 */
    { "101230509", "\xe6\x99\x8b\xe6\xb1\x9f\xe5\xb8\x82" },              /* 晋江市 */
    { "101230510", "\xe7\x9f\xb3\xe7\x8b\xae\xe5\xb8\x82" },              /* 石狮市 */
    { "101230508", "\xe5\x8d\x97\xe5\xae\x89\xe5\xb8\x82" },              /* 南安市 */
    { "101230511", "\xe6\x83\xa0\xe5\xae\x89\xe5\x8e\xbf" },              /* 惠安县 */
    { "101230507", "\xe5\xae\x89\xe6\xba\xaa\xe5\x8e\xbf" },              /* 安溪县 */
    { "101230505", "\xe6\xb0\xb8\xe6\x98\xa5\xe5\x8e\xbf" },              /* 永春县 */
    { "101230504", "\xe5\xbe\xb7\xe5\x8c\x96\xe5\x8e\xbf" },              /* 德化县 */
    { "101230501", "\xe6\xb3\x89\xe5\xb7\x9e\xe7\xbb\x8f\xe6\xb5\x8e\xe6\x8a\x80\xe6\x9c\xaf\xe5\xbc\x80\xe5\x8f\x91\xe5\x8c\xba" },  /* 泉州经济技术开发区 */
    { "101230501", "\xe6\xb3\x89\xe5\xb7\x9e\xe5\x8f\xb0\xe5\x95\x86\xe6\x8a\x95\xe8\xb5\x84\xe5\x8c\xba" },  /* 泉州台商投资区 */
};
static const int s_location_count = sizeof(s_locations) / sizeof(s_locations[0]);

/* 运行时地区 ID 和名称 */
static char s_location_id[16] = DEFAULT_LOCATION;
static char s_location_name[32] = "";

static void load_location(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t sz = sizeof(s_location_id);
    if (nvs_get_str(h, "location_id", s_location_id, &sz) != ESP_OK) {
        strcpy(s_location_id, DEFAULT_LOCATION);
    }
    sz = sizeof(s_location_name);
    if (nvs_get_str(h, "location_name", s_location_name, &sz) != ESP_OK) {
        s_location_name[0] = '\0';
    }
    nvs_close(h);
    ESP_LOGI(TAG, "Loaded location: %s (%s)", s_location_name, s_location_id);
}

static void save_location(const char *id, const char *name) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "location_id", id);
    nvs_set_str(h, "location_name", name);
    nvs_commit(h);
    nvs_close(h);
}

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
    static bool hooks_set = false;
    if (!hooks_set) {
        cJSON_Hooks hooks = { cjson_malloc, cjson_free };
        cJSON_InitHooks(&hooks);
        hooks_set = true;
    }
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        ESP_LOGW(TAG, "JSON fail len=%d err=%s", (int)strlen(body), err ? err : "?");
        ESP_LOGW(TAG, "RAW: %.80s", body);
        return;
    }
    cJSON *code = cJSON_GetObjectItem(root, "code");
    const char *cs = cJSON_IsString(code) ? code->valuestring : NULL;
    if (!cs || strcmp(cs, "200")) { cJSON_Delete(root); return; }
    /* City name: use user-selected location name if available, else API response */
    const char *city = s_location_name[0] ? s_location_name : "\xe6\xb3\x89\xe5\xb7\x9e";
    cJSON *loc = cJSON_GetObjectItem(root, "location");
    if (loc && !s_location_name[0]) {
        cJSON *adm2 = cJSON_GetObjectItem(loc, "adm2");
        cJSON *name = cJSON_GetObjectItem(loc, "name");
        if (cJSON_IsString(adm2) && adm2->valuestring[0])
            city = adm2->valuestring;
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
            home_screen_update_weather(city, text->valuestring, atoi(temp->valuestring), s_today_high, s_today_low, NULL);
            lvgl_unlock();
        }
    }
    cJSON *d = cJSON_GetObjectItem(root, "daily");
    if (d && cJSON_IsArray(d)) {
        int n = cJSON_GetArraySize(d); if(n>4)n=4;
        ESP_LOGI(TAG, "Daily data: %d days, high=%s low=%s",
            n,
            n>0?cJSON_GetObjectItem(cJSON_GetArrayItem(d,0),"tempMax")->valuestring:"?",
            n>0?cJSON_GetObjectItem(cJSON_GetArrayItem(d,0),"tempMin")->valuestring:"?");
        lvgl_lock();
        /* Update today's high/low from daily[0] */
        if (n > 0) {
            cJSON *d0 = cJSON_GetArrayItem(d, 0);
            if (d0) {
                cJSON *dhi = cJSON_GetObjectItem(d0, "tempMax");
                cJSON *dlo = cJSON_GetObjectItem(d0, "tempMin");
                if (dhi) s_today_high = atoi(dhi->valuestring);
                if (dlo) s_today_low  = atoi(dlo->valuestring);
                ESP_LOGI(TAG, "Today range: %d~%dC", s_today_low, s_today_high);
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
            ESP_LOGI(TAG, "Forecast[%d]: %s %s %s~%sC",
                i-1, date_str, dtext?dtext->valuestring:"?",
                dmin?dmin->valuestring:"?", dmax?dmax->valuestring:"?");
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
            ESP_LOGI(TAG, "Resp: len=%d gzip=%d first=0x%02X%02X%02X%02X",
                (unsigned)s_resp_len,
                (s_resp_len>=2 && s_resp[0]==0x1F && s_resp[1]==0x8B)?1:0,
                (unsigned)s_resp[0],(unsigned)s_resp[1],(unsigned)s_resp[2],(unsigned)s_resp[3]);
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
    if (s_api_key[0] == '\0') {
        ESP_LOGE(TAG, "API key not set — skipping weather request");
        return ESP_ERR_INVALID_STATE;
    }
    char path[320];
    snprintf(path, sizeof(path), "%s?location=%s&key=%s&gzip=n", api_path, s_location_id, s_api_key);
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
        if (s_skip_weather) {
            /* 无 API 密钥 → 等待通知（可能被 weather_set_api_key 唤醒） */
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_INTERVAL_SEC * 1000));
            continue;
        }
        if (wifi_manager_wait_connected(pdMS_TO_TICKS(WIFI_TIMEOUT_MS))) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            for (int r=0;r<3;r++){ if(wf("/v7/weather/now")==0)break; vTaskDelay(pdMS_TO_TICKS(5000)); }
            vTaskDelay(pdMS_TO_TICKS(2000));
            for (int r=0;r<3;r++){ if(wf("/v7/weather/7d")==0)break; vTaskDelay(pdMS_TO_TICKS(5000)); }
        }
        /* 等待 30 分钟，但允许被 weather_request_refresh() 提前唤醒 */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_INTERVAL_SEC * 1000));
    }
}

esp_err_t weather_init(void) {
    if (s_weather_task) return ESP_OK;
    load_location();

    /* ── API 密钥：优先从 Kconfig 读取（CONFIG_WEATHER_API_KEY）── */
    /*     密钥编译时注入，不在 git 源码里；也可运行时通过 weather_set_api_key() 设置后存入 NVS */
    nvs_handle_t h;
    bool has_key = false;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        size_t sz = sizeof(s_api_key);
        if (nvs_get_str(h, "api_key", s_api_key, &sz) == ESP_OK && s_api_key[0]) {
            s_skip_weather = false;
            has_key = true;
            ESP_LOGI(TAG, "API key loaded from NVS");
        }
#ifdef CONFIG_WEATHER_API_KEY
        if (!has_key && CONFIG_WEATHER_API_KEY[0]) {
            strncpy(s_api_key, CONFIG_WEATHER_API_KEY, sizeof(s_api_key) - 1);
            s_api_key[sizeof(s_api_key) - 1] = '\0';
            nvs_set_str(h, "api_key", s_api_key);
            nvs_commit(h);
            s_skip_weather = false;
            has_key = true;
            ESP_LOGI(TAG, "API key from Kconfig migrated to NVS");
        }
#endif
        nvs_close(h);
    }
    if (!has_key) {
        ESP_LOGW(TAG, "API key not set — weather disabled. Configure it via 'idf.py menuconfig' or weather_set_api_key()");
    }
    xTaskCreatePinnedToCore(weather_task,"weather",4096,NULL,2,&s_weather_task,0);
    ESP_LOGI(TAG,"started, location=%s (%s) api=%s",
             s_location_name, s_location_id, s_skip_weather ? "MISSING" : "ok");
    return ESP_OK;
}

void weather_set_api_key(const char *key) {
    if (!key || !key[0]) return;
    strncpy(s_api_key, key, sizeof(s_api_key) - 1);
    s_api_key[sizeof(s_api_key) - 1] = '\0';
    s_skip_weather = false;
    /* 持久化到 NVS */
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "api_key", s_api_key);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "API key updated");
    weather_request_refresh();
}

void weather_set_location(const char *loc) {
    if (!loc) return;
    /* 按 ID 匹配（可能重复，只更新 ID，名称保留或按列表更新） */
    strncpy(s_location_id, loc, sizeof(s_location_id) - 1);
    s_location_id[sizeof(s_location_id) - 1] = '\0';
    s_location_name[0] = '\0';
    for (int i = 0; i < s_location_count; i++) {
        if (strcmp(s_locations[i].id, loc) == 0) {
            strncpy(s_location_name, s_locations[i].name, sizeof(s_location_name) - 1);
            s_location_name[sizeof(s_location_name) - 1] = '\0';
            break;
        }
    }
    save_location(s_location_id, s_location_name);
    ESP_LOGI(TAG, "location set: %s (%s)", s_location_name, s_location_id);
}

void weather_set_location_by_index(int index) {
    if (index < 0 || index >= s_location_count) return;
    strncpy(s_location_id, s_locations[index].id, sizeof(s_location_id) - 1);
    s_location_id[sizeof(s_location_id) - 1] = '\0';
    strncpy(s_location_name, s_locations[index].name, sizeof(s_location_name) - 1);
    s_location_name[sizeof(s_location_name) - 1] = '\0';
    save_location(s_location_id, s_location_name);
    ESP_LOGI(TAG, "location set by index %d: %s (%s)", index, s_location_name, s_location_id);
}

void weather_request_refresh(void) {
    if (s_weather_task) {
        xTaskNotifyGive(s_weather_task);
        ESP_LOGI(TAG, "refresh requested");
    }
}

int weather_get_location_index(void) {
    for (int i = 0; i < s_location_count; i++) {
        if (strcmp(s_locations[i].id, s_location_id) == 0 &&
            strcmp(s_locations[i].name, s_location_name) == 0) {
            return i;
        }
    }
    return 0;
}

const char *weather_get_location_id(void) {
    return s_location_id;
}

const char *weather_get_location_name(void) {
    return s_location_name;
}

const weather_loc_entry_t *weather_get_locations(void) {
    return s_locations;
}

int weather_get_location_count(void) {
    return s_location_count;
}
