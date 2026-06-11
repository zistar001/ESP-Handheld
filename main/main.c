#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* BSP */
#include "bsp_board.h"
#include "bsp/key_driver.h"
#include "bsp/sd_card.h"

/* UI */
#include "ui/display_driver.h"
#include "ui/screens/settings_screen.h"
#include "ui/screens/airmouse_screen.h"
#include "ui/screens/kbd_screen.h"
#include "ui/screens/ip_input.h"

/* App framework */
#include "app/app_manager.h"
#include "app/menu.h"
#include "app/rom_browser.h"

/* Modules */
#include "modules/nes/nes_wrapper.h"
#include "modules/settings/settings_manager.h"
#include "modules/audio/box_audio_codec.h"
#include "modules/sensor/aht20_driver.h"
#include "modules/imu/imu_driver.h"
#include "modules/pc_remote/air_mouse.h"
#include "modules/pc_remote/ble_hid.h"
#include "modules/pc_remote/wifi_audio.h"
#include "modules/power/battery_monitor.h"

#include "modules/wifi_manager/wifi_manager.h"
#include "modules/weather/weather.h"
#include "modules/time_sync/time_sync.h"

static const char *TAG = "MAIN";

/* ================================================================
 * settings_sync_global — apply settings to hardware
 * ================================================================ */
void settings_sync_global(void) {
    settings_t s;
    if (settings_load(&s) != ESP_OK) return;
    bsp_lcd_backlight_set(s.brightness);
    box_audio_set_volume(s.volume);
}

/* ================================================================
 * IMU task — read sensor periodically, log attitude for debugging
 * ================================================================ */
static void imu_display_task(void *arg) {
    imu_data_t imu;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        if (imu_read(&imu) == ESP_OK) {
            /* Compute pitch & roll from accelerometer */
            float ax = imu.ax, ay = imu.ay, az = imu.az;
            float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.29578f;
            float roll  = atan2f(ay, az) * 57.29578f;

            ESP_LOGD("IMU", "a=(%.2f,%.2f,%.2f)g g=(%.1f,%.1f,%.1f)dps p/r=%.0f/%.0f",
                     imu.ax, imu.ay, imu.az,
                     imu.gx, imu.gy, imu.gz,
                     pitch, roll);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100)); /* 10 Hz */
    }
}

/* ================================================================
 * Key handler — routes based on app_manager state
 * ================================================================ */
static void key_handler(key_id_t key, bool pressed) {
    app_state_t state = app_manager_get_state();

    switch (state) {
        case APP_STATE_LAUNCHER:
            if (pressed && key == KEY_START) {
                lvgl_lock();
                menu_enter();
                lvgl_unlock();
            }
            break;

        case APP_STATE_MENU:
            if (pressed) {
                lvgl_lock();
                switch (key) {
                    case KEY_UP:    menu_navigate(0, -1); break;
                    case KEY_DOWN:  menu_navigate(0, 1);  break;
                    case KEY_LEFT:  menu_navigate(-1, 0); break;
                    case KEY_RIGHT: menu_navigate(1, 0);  break;
                    case KEY_A:     menu_select();        break;
                    case KEY_B:
                    case KEY_START: app_manager_return(); break;
                    default: break;
                }
                lvgl_unlock();
            }
            break;

        case APP_STATE_RUNNING:
            if (app_manager_get_current_app() == APP_ID_NES) {
                rom_browser_key(key, pressed);
            } else if (app_manager_get_current_app() == APP_ID_KEYBOARD) {
                /* Keyboard HID app */
                static TickType_t start_tick = 0;

                if (key == KEY_START) { if (pressed) start_tick = xTaskGetTickCount(); break; }

                /* B: ESC or START+B combo exit */
                if (key == KEY_B) {
                    if (pressed) {
                        if ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(500)) {
                            wifi_audio_voice_stop();
                            ble_hid_release_all(); lvgl_lock(); app_manager_return(); lvgl_unlock();
                        } else if (kbd_screen_is_enabled()) {
                            ble_hid_send_key(0, 0x29); /* B = ESC */
                        }
                    }
                    break;
                }

                /* A: PTT — press=speak, release=stop. No voice → Enter */
                if (key == KEY_A) {
                    if (pressed && kbd_screen_is_enabled()) {
                        if (wifi_audio_voice_start() == ESP_OK)
                            { lvgl_lock(); kbd_screen_set_voice_active(true); lvgl_unlock(); }
                    } else if (!pressed) {
                        if (wifi_audio_is_streaming()) {
                            wifi_audio_voice_stop();
                        }
                        bool toggle = !kbd_screen_is_enabled();
                        lvgl_lock();
                        kbd_screen_set_voice_active(false);
                        if (toggle) kbd_screen_select();
                        lvgl_unlock();
                    }
                    break;
                }

                if (!pressed) break;
                if (kbd_screen_is_enabled()) {
                    uint8_t hid = 0;
                    switch (key) {
                        case KEY_UP:    hid = 0x52; break;
                        case KEY_DOWN:  hid = 0x51; break;
                        case KEY_LEFT:  hid = 0x50; break;
                        case KEY_RIGHT: hid = 0x4F; break;
                        default: break;
                    }
                    if (hid) ble_hid_send_key(0, hid);
                }

            } else if (app_manager_get_current_app() == APP_ID_PC_REMOTE) {
                /* Air Mouse app */
                static TickType_t start_tick = 0;
                if (key == KEY_START) { if (pressed) start_tick = xTaskGetTickCount(); break; }
                if (key == KEY_B) {
                    if (pressed) {
                        if ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(500)) {
                            ble_hid_release_all(); lvgl_lock(); app_manager_return(); lvgl_unlock();
                        } else if (airmouse_screen_mouse_enabled()) {
                            ble_hid_mouse_click(0x02); /* B = right click */
                        }
                    }
                    break;
                }
                if (!pressed) break;
                if (airmouse_screen_mouse_enabled()) {
                    /* Mouse ON: A=left click, B=right click, UI nav still works */
                    if (key == KEY_A) {
                        ble_hid_mouse_click(0x01);
                        /* Also change sensitivity if focused on a sens button */
                        lvgl_lock(); airmouse_screen_select(); lvgl_unlock();
                        break;
                    }
                    /* UP/DOWN still navigate UI (sensitivity) */
                    lvgl_lock();
                    if (key == KEY_UP) airmouse_screen_navigate(0, -1);
                    else if (key == KEY_DOWN) airmouse_screen_navigate(0, 1);
                    lvgl_unlock();
                } else {
                    /* Mouse OFF: keys navigate UI toggle + sensitivity */
                    lvgl_lock();
                    if (key == KEY_UP)        airmouse_screen_navigate(0, -1);
                    else if (key == KEY_DOWN)  airmouse_screen_navigate(0, 1);
                    else if (key == KEY_A)     airmouse_screen_select();
                    lvgl_unlock();
                }
                air_mouse_set_enabled(airmouse_screen_mouse_enabled());
            } else if (app_manager_get_current_app() == APP_ID_SETTINGS && pressed) {
                lvgl_lock();
                switch (key) {
                    case KEY_UP:    settings_screen_navigate(-1); break;
                    case KEY_DOWN:  settings_screen_navigate(1);  break;
                    case KEY_A:     settings_screen_select();     break;
                    case KEY_B:
                    case KEY_START: app_manager_return(); break;
                    default: break;
                }
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_IP_INPUT && pressed) {
                lvgl_lock();
                if (key == KEY_UP)        ip_input_navigate(0, 1);
                else if (key == KEY_DOWN)  ip_input_navigate(0, -1);
                else if (key == KEY_A)     ip_input_select();
                else if (key == KEY_B || key == KEY_START) { ip_input_cancel(); app_manager_return(); }
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_WIFI_SETUP && pressed) {
                lvgl_lock();
                if (key == KEY_A) {
                    ble_hid_deinit(); /* stop BLE to free 2.4GHz for WiFi AP */
                    wifi_manager_forget_ssids();
                    wifi_manager_stop_station();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    wifi_manager_start_config();
                    app_manager_launch(APP_ID_WIFI_SETUP); /* refresh screen */
                } else if (key == KEY_B || key == KEY_START) {
                    app_manager_return();
                }
                lvgl_unlock();
            } else if (pressed && (key == KEY_B || key == KEY_START)) {
                lvgl_lock();
                app_manager_return();
                lvgl_unlock();
            }
            break;
    }
}

/* ================================================================
 * Entry point
 * ================================================================ */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 Smart Handheld ===");

    /* 1. NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* 2. Settings (NVS-backed) */
    settings_init();
    wifi_audio_load_settings();
    settings_t cfg;
    settings_load(&cfg);
    ESP_LOGI(TAG, "Settings loaded: vol=%d bright=%d", cfg.volume, cfg.brightness);

    /* 3. SD card first (creates SPI bus at safe speed, LCD joins later) */
    ret = sd_card_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "SD card init failed (continuing): %s", esp_err_to_name(ret));

    /* 4. BSP: LCD joins existing SPI bus */
    bsp_board_init();
    bsp_lcd_backlight_set(cfg.brightness);

    /* 5. LVGL display driver */
    ui_display_init();

    /* 6. Audio codec (ES8311 + ES7210) */
    box_audio_init();

    /* 7. Sensors (best-effort) */
    ret = aht20_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "AHT20 init failed (continuing)");
    ret = imu_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IMU init failed (continuing)");
    } else {
        xTaskCreatePinnedToCore(imu_display_task, "imu", 3072, NULL, 3, NULL, 0);
        ESP_LOGI(TAG, "IMU display task created");
    }

    /* 8. Battery monitor (may fail due to ADC conflict) */
    ret = battery_monitor_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "Battery monitor init failed (continuing)");

    /* 9. BLE HID — start advertising (best-effort, needs BT enabled in sdkconfig) */
    ret = ble_hid_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "BLE HID init failed (continuing)");
    else {
        ESP_LOGI(TAG, "BLE HID advertising as 'ESP-Handheld'");
        /* 10. Air mouse — task runs but disabled; enabled by Remote app */
        if (imu_is_ready()) {
            air_mouse_init();
            air_mouse_set_enabled(false);
            xTaskCreatePinnedToCore(air_mouse_task, "airmouse", 3072, NULL, 3, NULL, 0);
            ESP_LOGI(TAG, "Air mouse task created (disabled)");
        }
    }

    /* 10. Apply settings to hardware */
    settings_sync_global();

    /* 14. NES wrapper (allocate memory, create video task) */
    nes_wrapper_init();

    /* 11. WiFi manager (independent mode) */
    ret = wifi_manager_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "WiFi init failed (continuing): %s", esp_err_to_name(ret));

    /* 12. Weather (background task) */
    weather_init();

    /* 13. Time sync (SNTP) */
    time_sync_init();


    /* 15. Key driver */
    key_driver_init(key_handler);
    xTaskCreatePinnedToCore(key_driver_scan_task, "key", 4096, NULL, 2, NULL, 0);

    /* 16. App manager — shows launcher/home screen */
    app_manager_init();

    ESP_LOGI(TAG, "System ready. Idle loop.");

    /* Idle: keep main task alive */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
