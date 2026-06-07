#include "wifi_bridge.h"
#include <esp-wifi-connect/include/wifi_manager.h>
#include <esp-wifi-connect/include/ssid_manager.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <nvs.h>

static const char *TAG = "WIFI_BRIDGE";

#define CONNECTED_BIT BIT0

static EventGroupHandle_t s_events = NULL;
static bool s_inited = false;
static bool s_netif_inited = false;
static char s_ip_str[16] = "";
static char s_ssid_str[33] = "";
static bool s_station_started = false;

static void on_wifi_event(WifiEvent event, const std::string &data) {
    switch (event) {
        case WifiEvent::Connected:
            ESP_LOGI(TAG, "WiFi connected: %s", data.c_str());
            if (s_events) xEventGroupSetBits(s_events, CONNECTED_BIT);
            {
                auto &wifi = WifiManager::GetInstance();
                strncpy(s_ip_str, wifi.GetIpAddress().c_str(), sizeof(s_ip_str) - 1);
                strncpy(s_ssid_str, wifi.GetSsid().c_str(), sizeof(s_ssid_str) - 1);
            }
            break;
        case WifiEvent::Disconnected:
            ESP_LOGW(TAG, "WiFi disconnected");
            if (s_events) xEventGroupClearBits(s_events, CONNECTED_BIT);
            s_ip_str[0] = '\0';
            s_ssid_str[0] = '\0';
            break;
        case WifiEvent::ConfigModeEnter:
            ESP_LOGI(TAG, "Config mode entered, AP: %s", data.c_str());
            break;
        case WifiEvent::ConfigModeExit:
            ESP_LOGI(TAG, "Config mode exited");
            break;
        default:
            break;
    }
}

esp_err_t wifi_bridge_init(const char *ssid_prefix, const char *lang) {
    if (s_inited) return ESP_OK;

    if (!s_netif_inited) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_netif_inited = true;
    }

    if (!s_events) s_events = xEventGroupCreate();

    auto &wifi = WifiManager::GetInstance();

    WifiManagerConfig config;
    config.ssid_prefix = ssid_prefix ? ssid_prefix : "ESP-Handheld";
    config.language = lang ? lang : "zh-CN";

    if (!wifi.Initialize(config)) {
        ESP_LOGE(TAG, "WifiManager::Initialize failed");
        return ESP_FAIL;
    }

    wifi.SetEventCallback(on_wifi_event);

    /* Migrate legacy SSID from settings_manager NVS if SsidManager is empty */
    auto &ssid_mgr = SsidManager::GetInstance();
    if (ssid_mgr.GetSsidList().empty()) {
        nvs_handle_t nvs;
        if (nvs_open("settings", NVS_READONLY, &nvs) == ESP_OK) {
            char ssid[32] = {0}, pass[64] = {0};
            size_t len = sizeof(ssid);
            if (nvs_get_str(nvs, "wifi_ssid", ssid, &len) == ESP_OK && strlen(ssid) > 0) {
                len = sizeof(pass);
                nvs_get_str(nvs, "wifi_pass", pass, &len);
                ssid_mgr.AddSsid(ssid, pass);
                ESP_LOGI(TAG, "Migrated legacy WiFi config: %s", ssid);
            }
            nvs_close(nvs);
        }
    }

    s_inited = true;
    return ESP_OK;
}

bool wifi_bridge_is_connected(void) {
    return WifiManager::GetInstance().IsConnected();
}

void wifi_bridge_start_station(void) {
    if (!s_inited) return;
    if (s_station_started) return;
    s_station_started = true;
    WifiManager::GetInstance().StartStation();
    ESP_LOGI(TAG, "Station mode started");
}

void wifi_bridge_stop_station(void) {
    if (!s_inited) return;
    s_station_started = false;
    WifiManager::GetInstance().StopStation();
}

int wifi_bridge_get_rssi(void) {
    return WifiManager::GetInstance().GetRssi();
}

const char *wifi_bridge_get_ip(void) {
    return s_ip_str;
}

const char *wifi_bridge_get_ssid(void) {
    return s_ssid_str;
}

void wifi_bridge_forget_ssids(void) {
    auto &ssid_mgr = SsidManager::GetInstance();
    ssid_mgr.Clear();
    ;
    wifi_bridge_stop_station();
    s_ip_str[0] = '\0';
    s_ssid_str[0] = '\0';
    ESP_LOGI(TAG, "All saved SSIDs cleared");
}

bool wifi_bridge_wait_connected(TickType_t timeout) {
    if (WifiManager::GetInstance().IsConnected()) return true;
    EventBits_t bits = xEventGroupWaitBits(s_events, CONNECTED_BIT,
                                           pdTRUE, pdFALSE, timeout);
    return (bits & CONNECTED_BIT) != 0;
}

bool wifi_bridge_has_saved_ssid(void) {
    return !SsidManager::GetInstance().GetSsidList().empty();
}

void wifi_bridge_add_ssid(const char *ssid, const char *password) {
    SsidManager::GetInstance().AddSsid(ssid, password ? password : "");
}

void wifi_bridge_start_config(void) {
    WifiManager::GetInstance().StartConfigAp();
}

void wifi_bridge_stop_config(void) {
    WifiManager::GetInstance().StopConfigAp();
}

bool wifi_bridge_is_config_mode(void) {
    return WifiManager::GetInstance().IsConfigMode();
}

const char *wifi_bridge_get_ap_ssid(void) {
    static std::string cached;
    cached = WifiManager::GetInstance().GetApSsid();
    return cached.c_str();
}

const char *wifi_bridge_get_ap_url(void) {
    static std::string cached;
    cached = WifiManager::GetInstance().GetApWebUrl();
    return cached.c_str();
}
