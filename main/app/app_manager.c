#include "app_manager.h"
#include "launcher.h"
#include "menu.h"
#include "rom_browser.h"
#include "modules/nes/nes_wrapper.h"
#include "modules/settings/settings_manager.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "modules/pc_remote/air_mouse.h"
#include "modules/pc_remote/wifi_audio.h"
#include "ui/screens/settings_screen.h"
#include "ui/screens/countdown_screen.h"
#include "ui/screens/airmouse_screen.h"
#include "ui/screens/kbd_screen.h"
#include "ui/screens/ip_input.h"
#include "ui/display_driver.h"
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
    { APP_ID_KEYBOARD, "Kbd", "K" },
    { APP_ID_MOUSE, "Mouse", "M" },
    { APP_ID_COUNTDOWN, "Timer", "T" },
    { APP_ID_FORTUNE, "Coming", "F" },
    { APP_ID_RECORDER, "Coming", "R" },
    { APP_ID_SETTINGS, "Config", "S" },
};

void app_manager_init(void) { state = APP_STATE_LAUNCHER; cur_app = APP_ID_NONE; launcher_enter(); }
void app_manager_set_state(app_state_t s) { state = s; }
app_state_t app_manager_get_state(void) { return state; }
app_id_t app_manager_get_current_app(void) { return cur_app; }

static void wifi_do_reset(void) {
    ESP_LOGI(TAG, "WiFi reset + config mode");
    wifi_manager_forget_ssids();
    wifi_manager_stop_station();
    vTaskDelay(pdMS_TO_TICKS(500));
    wifi_manager_start_config();
    /* Don't re-launch — let existing WiFi setup screen show the AP info */
}

void app_manager_wifi_action(void) {
    wifi_do_reset();
}

static void wifi_reset_cb(lv_event_t *e) {
    (void)e;
    wifi_do_reset();
}

esp_err_t app_manager_launch(app_id_t id) {
    if (id < 0 || id >= APP_ID_MAX) return ESP_ERR_INVALID_ARG;
    cur_app = id; state = APP_STATE_RUNNING;
    const char *name = "?";
    for (int i = 0; i < (int)(sizeof(apps)/sizeof(apps[0])); i++)
        if (apps[i].id == id) { name = apps[i].name; break; }
    ESP_LOGI(TAG, "Launch: %s", name);

    switch (id) {
        case APP_ID_NES:
            rom_browser_enter();
            break;
        case APP_ID_KEYBOARD:
            ESP_LOGI(TAG, "Keyboard HID");
            air_mouse_set_enabled(false);
            kbd_screen_create();
            break;
        case APP_ID_MOUSE:
            ESP_LOGI(TAG, "Air Mouse");
            air_mouse_set_enabled(false);
            airmouse_screen_create();
            break;
        case APP_ID_FORTUNE:
        case APP_ID_RECORDER:
            ESP_LOGI(TAG, "Coming soon");
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
            lv_obj_t *old = lv_scr_act();
            lv_obj_t *scr = lv_obj_create(NULL);
            lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
            lv_obj_add_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *title = lv_label_create(scr);
            lv_label_set_text(title, "WiFi Settings (B: Back)");
            lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_pos(title, 10, 10);

            lv_obj_t *status = lv_label_create(scr);
            lv_obj_set_style_text_color(status, lv_color_hex(0xCCCCCC), 0);
            lv_obj_set_pos(status, 10, 45);

            char buf[256];
            int y = 45;
            if (wifi_manager_is_connected()) {
                snprintf(buf, sizeof(buf),
                    "Connected\nSSID: %s\nIP: %s\nRSSI: %d dBm",
                    wifi_manager_get_ssid(), wifi_manager_get_ip(),
                    wifi_manager_get_rssi());
                y = 105;
            } else if (wifi_manager_is_config_mode()) {
                snprintf(buf, sizeof(buf),
                    "Config AP:\nSSID: %s\nURL: %s\n\n"
                    "Phone: connect to AP\n"
                    "Browser: open URL",
                    wifi_manager_get_ap_ssid(), wifi_manager_get_ap_url());
                y = 160;
            } else if (wifi_manager_has_saved_ssid()) {
                snprintf(buf, sizeof(buf),
                    "Connecting...\nSaved network found.");
                wifi_manager_start_station();
                y = 95;
            } else {
                snprintf(buf, sizeof(buf),
                    "No WiFi configured.\nPress Setup to start AP\n"
                    "then connect phone & open URL.");
                y = 105;
            }
            lv_label_set_text(status, buf);

            lv_obj_t *btn = lv_btn_create(scr);
            lv_obj_set_size(btn, 220, 36);
            lv_obj_set_pos(btn, 10, y + 5);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF5C00), 0);
            lv_obj_set_style_radius(btn, 6, 0);
            lv_obj_t *btn_lbl = lv_label_create(btn);
            lv_label_set_text(btn_lbl, wifi_manager_is_connected() ?
                              "Reconfigure WiFi" : "WiFi Setup");
            lv_obj_center(btn_lbl);
            lv_obj_add_event_cb(btn, wifi_reset_cb, LV_EVENT_CLICKED, NULL);

            lv_scr_load(scr);
            if (old) lv_obj_del(old);
            break;
        }
        case APP_ID_COUNTDOWN:
            ESP_LOGI(TAG, "Countdown timer");
            countdown_screen_create();
            break;
        case APP_ID_IP_INPUT:
            ESP_LOGI(TAG, "PC IP input");
            ip_input_screen_create();
            break;
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
        if (cur_app == APP_ID_MOUSE || cur_app == APP_ID_KEYBOARD) {
            air_mouse_set_enabled(false);
            wifi_audio_voice_stop();
        }
        cur_app = APP_ID_NONE;
        state = APP_STATE_MENU; menu_enter();
    } else if (state == APP_STATE_MENU) {
        state = APP_STATE_LAUNCHER; launcher_enter();
    }
}

const app_entry_t *app_manager_get_apps(void) { return apps; }
int app_manager_get_app_count(void) { return sizeof(apps)/sizeof(apps[0]); }

