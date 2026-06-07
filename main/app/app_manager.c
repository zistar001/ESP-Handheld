#include "app_manager.h"
#include "launcher.h"
#include "menu.h"
#include "rom_browser.h"
#include "modules/nes/nes_wrapper.h"
#include "modules/settings/settings_manager.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "ui/screens/settings_screen.h"
#include "ui/screens/countdown_screen.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "lvgl.h"

static const char *TAG = "APP_MGR";
static app_state_t state = APP_STATE_LAUNCHER;
static app_id_t cur_app = APP_ID_NONE;

static const app_entry_t apps[] = {
    { APP_ID_NES, "NES", "1" },
    { APP_ID_XIAOZHI, "AI", "2" },
    { APP_ID_PC_REMOTE, "\xE9\x94\xAE\xE9\xBC\xA0", "3" },
    { APP_ID_SETTINGS, "\xE8\xAE\xBE\xE7\xBD\xAE", "4" },
    { APP_ID_WIFI_SETUP, "WiFi", "5" },
    { APP_ID_ABOUT, "\xE5\x85\xB3\xE4\xBA\x8E", "6" },
    { APP_ID_COUNTDOWN, "\xE5\x80\x92\xE8\xAE\xA1\xE6\x97\xB6", "7" },
};

void app_manager_init(void) { state = APP_STATE_LAUNCHER; cur_app = APP_ID_NONE; launcher_enter(); }
void app_manager_set_state(app_state_t s) { state = s; }
app_state_t app_manager_get_state(void) { return state; }
app_id_t app_manager_get_current_app(void) { return cur_app; }

esp_err_t app_manager_launch(app_id_t id) {
    if (id < 0 || id >= APP_ID_MAX) return ESP_ERR_INVALID_ARG;
    cur_app = id; state = APP_STATE_RUNNING;
    ESP_LOGI(TAG, "Launch: %s", apps[id].name);

    switch (id) {
        case APP_ID_NES:
            rom_browser_enter();
            break;
        case APP_ID_XIAOZHI: {
            ESP_LOGI(TAG, "Switching to XiaoZhi AI via partition reboot");
            const esp_partition_t *running = esp_ota_get_running_partition();
            if (!running) {
                ESP_LOGE(TAG, "Cannot detect running partition");
                break;
            }
            ESP_LOGI(TAG, "Running from: %s (subtype=%d)",
                     running->label, running->subtype);

            const esp_partition_t *target = NULL;
            if (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY ||
                running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
                target = esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP,
                    ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
            } else {
                target = esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP,
                    ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
            }

            if (target) {
                ESP_LOGI(TAG, "Switching to: %s", target->label);
                ESP_ERROR_CHECK(esp_ota_set_boot_partition(target));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "Target partition not found");
            }
            break;
        }
        case APP_ID_PC_REMOTE:
            ESP_LOGI(TAG, "PC remote not available (BLE removed)");
            break;
        case APP_ID_SETTINGS:
            ESP_LOGI(TAG, "Open settings");
            {
                settings_t s;
                settings_load(&s);
                settings_screen_create(&s);
            }
            break;
        case APP_ID_WIFI_SETUP: {
            ESP_LOGI(TAG, "WiFi setup");
            wifi_manager_init();
            lv_obj_t *scr = lv_obj_create(NULL);
            lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
            lv_obj_t *l = lv_label_create(scr);
            char buf[320];

            if (wifi_manager_is_connected()) {
                snprintf(buf, sizeof(buf),
                    "WiFi: Connected\n\n"
                    "SSID: %s\n"
                    "IP:   %s\n"
                    "RSSI: %d dBm\n\n"
                    "B: Back",
                    wifi_manager_get_ssid(),
                    wifi_manager_get_ip(),
                    wifi_manager_get_rssi());
            } else if (wifi_manager_is_config_mode()) {
                snprintf(buf, sizeof(buf),
                    "Config Mode Active\n\n"
                    "Connect to AP:\n%s\n\n"
                    "Open browser:\n%s\n\n"
                    "B: Back",
                    wifi_manager_get_ap_ssid(),
                    wifi_manager_get_ap_url());
            } else if (wifi_manager_has_saved_ssid()) {
                snprintf(buf, sizeof(buf),
                    "WiFi: Connecting...\n\n"
                    "Saved network found,\n"
                    "connecting in background.\n\n"
                    "To start config AP,\n"
                    "re-enter this menu.\n\n"
                    "B: Back");
                wifi_manager_start_station();
            } else {
                snprintf(buf, sizeof(buf),
                    "No WiFi configured\n\n"
                    "Starting web config mode...\n"
                    "Connect to AP:\n%s\n\n"
                    "Open browser:\n%s\n\n"
                    "B: Back",
                    wifi_manager_get_ap_ssid(),
                    wifi_manager_get_ap_url());
                wifi_manager_start_config();
            }

            lv_label_set_text(l, buf);
            lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
            lv_obj_center(l);
            lv_scr_load(scr);
            break;
        }
        case APP_ID_COUNTDOWN: {
            ESP_LOGI(TAG, "Countdown timer");
            countdown_screen_create();
            break;
        }
        case APP_ID_ABOUT: {
            ESP_LOGI(TAG, "Show about");
            const esp_partition_t *p = esp_ota_get_running_partition();
            const char *sys_name = "Game";
            if (p) {
                if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)
                    sys_name = "XiaoZhi AI";
                else if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0)
                    sys_name = "Game (OTA)";
            }
            lv_obj_t *scr = lv_obj_create(NULL);
            lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
            lv_obj_t *l = lv_label_create(scr);
            char buf[320];
            snprintf(buf, sizeof(buf),
                "ESP32-S3 Handheld\n\n"
                "System: %s\n"
                "Partition: %s\n"
                "IDF: " IDF_VER "\n"
                "Build: " __DATE__ " " __TIME__ "\n\n"
                "NES Emulator\n"
                "Weather (QWeather)\n"
                "Sensor: AHT20 + IMU",
                sys_name, p ? p->label : "unknown");
            lv_label_set_text(l, buf);
            lv_obj_set_style_text_color(l, lv_color_hex(0xCCCCCC), 0);
            lv_obj_center(l);
            lv_scr_load(scr);
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

void app_manager_return(void) {
    if (state == APP_STATE_RUNNING) {
        if (cur_app == APP_ID_NES) nes_stop();
        cur_app = APP_ID_NONE;
        state = APP_STATE_MENU; menu_enter();
    } else if (state == APP_STATE_MENU) {
        state = APP_STATE_LAUNCHER; launcher_enter();
    }
}

const app_entry_t *app_manager_get_apps(void) { return apps; }
int app_manager_get_app_count(void) { return sizeof(apps)/sizeof(apps[0]); }

