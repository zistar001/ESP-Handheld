#include "wifi_bridge.h"
#include <esp-wifi-connect/include/wifi_manager.h>
#include <esp-wifi-connect/include/ssid_manager.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>

static const char *TAG = "WIFI_BRIDGE";

#define CONNECTED_BIT BIT0

static EventGroupHandle_t s_events = NULL;
static bool s_inited = false;
static bool s_station_started = false;
static esp_timer_handle_t s_fallback_timer = NULL;
static char s_ip_str[16] = "";
static char s_ssid_str[33] = "";

/* Try to connect: if saved SSIDs exist, start station; else enter config mode */
static void try_wifi_connect(void) {
    auto &ssid_mgr = SsidManager::GetInstance();
    if (!ssid_mgr.GetSsidList().empty()) {
        ESP_LOGI(TAG, "Starting WiFi connection attempt");
        s_station_started = false;
        wifi_bridge_start_station();  /* will skip if already started */
    } else {
        ESP_LOGI(TAG, "No saved SSIDs, entering config mode");
        vTaskDelay(pdMS_TO_TICKS(1500));  /* let user see the message */
        WifiManager::GetInstance().StartConfigAp();
    }
}

static void fallback_to_config(void *arg) {
    (void)arg;
    ESP_LOGW(TAG, "WiFi connection timeout, entering config mode");
    WifiManager::GetInstance().StopStation();
    s_station_started = false;
    WifiManager::GetInstance().StartConfigAp();
}

static void cancel_fallback(void) {
    if (s_fallback_timer) esp_timer_stop(s_fallback_timer);
}

static void start_fallback(void) {
    if (!s_fallback_timer) {
        esp_timer_create_args_t t = {
            .callback = fallback_to_config,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "wifi_fb",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&t, &s_fallback_timer);
    }
    esp_timer_start_once(s_fallback_timer, 60 * 1000000);
}

static void on_wifi_event(WifiEvent event, const std::string &data) {
    switch (event) {
        case WifiEvent::Connected:
            ESP_LOGI(TAG, "WiFi connected: %s", data.c_str());
            cancel_fallback();
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
            cancel_fallback();
            break;
        case WifiEvent::ConfigModeExit:
            ESP_LOGI(TAG, "Config mode exited, trying connect");
            /* User submitted credentials via web. Try to connect. */
            try_wifi_connect();
            break;
        default:
            break;
    }
}

esp_err_t wifi_bridge_init(const char *ssid_prefix, const char *lang) {
    if (s_inited) return ESP_OK;

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
    start_fallback();
}

void wifi_bridge_stop_station(void) {
    if (!s_inited) return;
    s_station_started = false;
    cancel_fallback();
    WifiManager::GetInstance().StopStation();
}

int wifi_bridge_get_rssi(void) {
    return WifiManager::GetInstance().GetRssi();
}

const char *wifi_bridge_get_ip(void) { return s_ip_str; }
const char *wifi_bridge_get_ssid(void) { return s_ssid_str; }

void wifi_bridge_forget_ssids(void) {
    SsidManager::GetInstance().Clear();
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
