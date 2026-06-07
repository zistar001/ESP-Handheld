#include "wifi_manager.h"
#include "wifi_bridge.h"
#include "esp_log.h"

static const char *TAG = "WIFI_MGR";
static bool s_inited = false;

esp_err_t wifi_manager_init(void) {
    if (s_inited) return ESP_OK;
    esp_err_t ret = wifi_bridge_init("ESP-Handheld", "zh-CN");
    if (ret != ESP_OK) return ret;

    /* Auto-connect if we have saved credentials */
    if (wifi_bridge_has_saved_ssid()) {
        wifi_bridge_start_station();
        ESP_LOGI(TAG, "Saved WiFi found, auto-connecting...");
    } else {
        ESP_LOGI(TAG, "No saved WiFi, use setup to configure");
    }

    s_inited = true;
    ESP_LOGI(TAG, "WiFi manager ready");
    return ESP_OK;
}

void wifi_manager_start_station(void) {
    wifi_bridge_start_station();
}

void wifi_manager_stop_station(void) {
    wifi_bridge_stop_station();
}

bool wifi_manager_wait_connected(TickType_t timeout) {
    return wifi_bridge_wait_connected(timeout);
}

bool wifi_manager_is_connected(void) {
    return wifi_bridge_is_connected();
}

int wifi_manager_get_rssi(void) {
    return wifi_bridge_get_rssi();
}

const char *wifi_manager_get_ip(void) {
    return wifi_bridge_get_ip();
}

const char *wifi_manager_get_ssid(void) {
    return wifi_bridge_get_ssid();
}

void wifi_manager_forget_ssids(void) {
    wifi_bridge_forget_ssids();
}

void wifi_manager_start_config(void) {
    wifi_bridge_start_config();
}

void wifi_manager_stop_config(void) {
    wifi_bridge_stop_config();
}

bool wifi_manager_is_config_mode(void) {
    return wifi_bridge_is_config_mode();
}

bool wifi_manager_has_saved_ssid(void) {
    return wifi_bridge_has_saved_ssid();
}

const char *wifi_manager_get_ap_ssid(void) {
    return wifi_bridge_get_ap_ssid();
}

const char *wifi_manager_get_ap_url(void) {
    return wifi_bridge_get_ap_url();
}
