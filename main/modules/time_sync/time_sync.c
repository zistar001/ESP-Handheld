#include "time_sync.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "modules/settings/settings_manager.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <string.h>
#include <time.h>

static const char *TAG = "TIME_SYNC";

/* China Standard Time (UTC+8) */
#define TIMEZONE "CST-8"
#define NVS_NAMESPACE "time_sync"
#define NVS_KEY_TIME   "saved_time"

static bool s_synced = false;
static char s_time_buf[32] = "--:--";
static char s_date_buf[32] = "----/--/--";
static bool s_init_done = false;

static void on_time_synced(struct timeval *tv) {
    s_synced = true;
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    ESP_LOGI(TAG, "SNTP sync done: %04d-%02d-%02d %02d:%02d:%02d",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             ti.tm_hour, ti.tm_min, ti.tm_sec);

    /* Save to NVS for quick recovery on next boot */
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        int64_t ts = (int64_t)now;
        nvs_set_i64(h, NVS_KEY_TIME, ts);
        nvs_commit(h);
        nvs_close(h);
    }

    snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    snprintf(s_date_buf, sizeof(s_date_buf), "%04d/%02d/%02d",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
}

static void init_sntp(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(on_time_synced);
    esp_sntp_init();
}

static bool restore_from_nvs(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
        return false;

    int64_t ts = 0;
    esp_err_t err = nvs_get_i64(h, NVS_KEY_TIME, &ts);
    nvs_close(h);

    if (err != ESP_OK || ts <= 0)
        return false;

    /* Account for elapsed wall time since saved timestamp.
       This is a best-effort: we add the time since boot (monotonic).
       Not perfect but gives a close-to-realtime value after boot. */
    int64_t elapsed = esp_timer_get_time() / 1000000;
    time_t now = (time_t)(ts + elapsed);

    struct tm ti;
    localtime_r(&now, &ti);
    snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    snprintf(s_date_buf, sizeof(s_date_buf), "%04d/%02d/%02d",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);

    struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    ESP_LOGI(TAG, "Restored time from NVS: %s %s", s_date_buf, s_time_buf);
    return true;
}

esp_err_t time_sync_init(void) {
    if (s_init_done) return ESP_OK;
    s_init_done = true;

    setenv("TZ", TIMEZONE, 1);
    tzset();

    /* Try to restore last known time from NVS */
    restore_from_nvs();

    /* Always start SNTP — WiFi may connect via saved credentials later.
     * SNTP silently retries if WiFi isn't ready yet. */
    init_sntp();

    return ESP_OK;
}

bool time_sync_is_synced(void) { return s_synced; }

const char *time_sync_get_time_string(void) {
    /* If synced, regenerate from uptime clock for real-time accuracy */
    if (s_synced) {
        time_t now = time(NULL);
        if (now > 0) {
            struct tm ti;
            localtime_r(&now, &ti);
            snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            snprintf(s_date_buf, sizeof(s_date_buf), "%04d/%02d/%02d",
                     ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
        }
    }
    return s_time_buf;
}

const char *time_sync_get_date_string(void) {
    /* Same lazy refresh as get_time_string */
    if (s_synced) {
        time_t now = time(NULL);
        if (now > 0) {
            struct tm ti;
            localtime_r(&now, &ti);
            snprintf(s_time_buf, sizeof(s_time_buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            snprintf(s_date_buf, sizeof(s_date_buf), "%04d/%02d/%02d",
                     ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
        }
    }
    return s_date_buf;
}
