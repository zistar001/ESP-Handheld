#include "settings_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SETTINGS";
static const char *NVS_NS = "handheld";

static const settings_t defaults = {
    .volume = 80,
    .brightness = 50,
    .mic_gain = 70,
    .air_mouse_sens = 1.0f,
    .wifi_ssid = "",
    .wifi_pass = "",
    .sleep_enabled = true,
    .sleep_timeout_sec = 300,
    .key_map = {0, 1, 2, 3, 4, 5, 6},
};

esp_err_t settings_init(void) {
    /* Check if namespace exists with READONLY first */
    nvs_handle_t h;
    esp_err_t r = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (r == ESP_OK) {
        nvs_close(h);
        return ESP_OK;
    }
    if (r != ESP_ERR_NVS_NOT_FOUND) return r;

    /* First boot: create namespace and write defaults */
    ESP_LOGW(TAG, "First boot — creating defaults in NVS");
    r = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (r != ESP_OK) return r;

    nvs_set_u8(h, "volume", defaults.volume);
    nvs_set_u8(h, "bright", defaults.brightness);
    nvs_set_u8(h, "mic_gain", defaults.mic_gain);
    nvs_set_blob(h, "am_sens", &defaults.air_mouse_sens, sizeof(float));
    nvs_set_str(h, "wifi_ssid", defaults.wifi_ssid);
    nvs_set_str(h, "wifi_pass", defaults.wifi_pass);
    nvs_set_u8(h, "sleep_en", 1);
    nvs_set_u16(h, "sleep_to", defaults.sleep_timeout_sec);
    nvs_set_blob(h, "key_map", defaults.key_map, sizeof(defaults.key_map));
    r = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Default settings saved to NVS");
    return r;
}

esp_err_t settings_load(settings_t *s) {
    *s = defaults;
    nvs_handle_t h;
    esp_err_t r = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (r != ESP_OK) return r;

    nvs_get_u8(h, "volume", &s->volume);
    nvs_get_u8(h, "bright", &s->brightness);
    nvs_get_u8(h, "mic_gain", &s->mic_gain);
    size_t sz = sizeof(float);
    nvs_get_blob(h, "am_sens", &s->air_mouse_sens, &sz);
    sz = sizeof(s->wifi_ssid);
    nvs_get_str(h, "wifi_ssid", s->wifi_ssid, &sz);
    sz = sizeof(s->wifi_pass);
    nvs_get_str(h, "wifi_pass", s->wifi_pass, &sz);
    uint8_t slp = 1;
    nvs_get_u8(h, "sleep_en", &slp);
    s->sleep_enabled = slp;
    nvs_get_u16(h, "sleep_to", &s->sleep_timeout_sec);
    sz = sizeof(s->key_map);
    nvs_get_blob(h, "key_map", s->key_map, &sz);

    nvs_close(h);
    return ESP_OK;
}

esp_err_t settings_save(const settings_t *s) {
    nvs_handle_t h;
    esp_err_t r = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (r != ESP_OK) return r;

    nvs_set_u8(h, "volume", s->volume);
    nvs_set_u8(h, "bright", s->brightness);
    nvs_set_u8(h, "mic_gain", s->mic_gain);
    nvs_set_blob(h, "am_sens", &s->air_mouse_sens, sizeof(float));
    nvs_set_str(h, "wifi_ssid", s->wifi_ssid);
    nvs_set_str(h, "wifi_pass", s->wifi_pass);
    nvs_set_u8(h, "sleep_en", s->sleep_enabled ? 1 : 0);
    nvs_set_u16(h, "sleep_to", s->sleep_timeout_sec);
    nvs_set_blob(h, "key_map", s->key_map, sizeof(s->key_map));

    r = nvs_commit(h);
    nvs_close(h);
    return r;
}

esp_err_t settings_reset(void) {
    nvs_handle_t h;
    esp_err_t r = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (r != ESP_OK) return r;
    r = nvs_erase_all(h);
    if (r == ESP_OK) r = nvs_commit(h);
    nvs_close(h);
    return r;
}

void settings_print(const settings_t *s) {
    ESP_LOGI(TAG, "vol=%d bright=%d mic=%d am_sens=%.1f",
             s->volume, s->brightness, s->mic_gain, s->air_mouse_sens);
    ESP_LOGI(TAG, "wifi=%s sleep=%d timeout=%d",
             s->wifi_ssid, s->sleep_enabled, s->sleep_timeout_sec);
}
