#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

/* BSP */
#include "bsp_board.h"
#include "bsp/key_driver.h"
#include "bsp/sd_card.h"

/* UI */
#include "ui/display_driver.h"
#include "ui/screens/settings_screen.h"
#include "ui/screens/fortune_screen.h"
#include "ui/screens/liuren_screen.h"
#include "ui/screens/airmouse_screen.h"
#include "ui/screens/kbd_screen.h"
#include "ui/screens/ip_input.h"
#include "ui/screens/countdown_screen.h"
#include "ui/screens/calib_screen.h"
#include "ui/screens/theme_screen.h"
#include "ui/screens/spectrum_screen.h"

/* App framework */
#include "app/app_manager.h"
#include "app/menu.h"
#include "app/rom_browser.h"

/* Modules */
#include "modules/settings/settings_manager.h"
#include "modules/audio/box_audio_codec.h"
#include "modules/sensor/aht20_driver.h"
#include "modules/imu/imu_driver.h"
#include "modules/theme/theme_manager.h"
#include "modules/pc_remote/air_mouse.h"
#include "modules/pc_remote/ble_hid.h"
#include "modules/pc_remote/wifi_audio.h"
#include "modules/power/battery_monitor.h"

#include "modules/wifi_manager/wifi_manager.h"
#include "modules/weather/weather.h"
#include "modules/time_sync/time_sync.h"

static const char *TAG = "MAIN";

/* Sleep management */
static bool s_sleeping = false;
static TickType_t s_last_activity = 0;


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
 * Key handler — routes based on app_manager state
 * ================================================================ */
static void key_handler(key_id_t key, bool pressed) {
    if (pressed) {
        s_last_activity = xTaskGetTickCount();
        if (s_sleeping) {
            s_sleeping = false;
            settings_t s; settings_load(&s);
            bsp_lcd_backlight_set(s.brightness);
        }
    }

    /* Game exit checking removed — games now run on separate partition (ota_0) */
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

                if (key == KEY_START) {
                    if (pressed) {
                        start_tick = xTaskGetTickCount();
                        if (kbd_screen_is_enabled())
                            ble_hid_send_key(0, 0x29); /* START = ESC */
                    }
                    break;
                }

                /* B: Backspace or START+B combo exit */
                if (key == KEY_B) {
                    if (pressed) {
                        if ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(500)) {
                            wifi_audio_voice_stop();
                            ble_hid_release_all();
                            lvgl_lock(); kbd_screen_set_voice_active(false); lvgl_unlock();
                            lvgl_lock(); app_manager_return(); lvgl_unlock();
                        } else if (kbd_screen_is_enabled()) {
                            ble_hid_send_key(0, 0x2A); /* B = Backspace */
                        }
                    }
                    break;
                }

                /* RIGHT: GPIO polling PTT — hold=voice+alt+x, short=arrow */
                if (key == KEY_RIGHT) {
                    if (pressed) {
                        /* Wait 300ms to distinguish short press from hold */
                        vTaskDelay(pdMS_TO_TICKS(300));

                        if (gpio_get_level(BSP_KEY_RIGHT) != 0) {
                            /* Released before 300ms: short press - send RIGHT_ARROW */
                            if (kbd_screen_is_enabled())
                                ble_hid_send_key(0, 0x4F); /* RIGHT_ARROW */
                        } else {
                            /* Still held: enter PTT mode */
                            ble_hid_send_combo(0x03, 0x06);  /* Ctrl+Shift+C toggle ON */
                            wifi_audio_voice_start();
                            lvgl_lock(); kbd_screen_set_voice_active(true); lvgl_unlock();

                            /* Poll GPIO until key release */
                            while (gpio_get_level(BSP_KEY_RIGHT) == 0)
                                vTaskDelay(pdMS_TO_TICKS(50));

                            /* Released: stop PTT + alt+x toggle OFF */
                            ble_hid_send_combo(0x03, 0x06);  /* Ctrl+Shift+C toggle OFF */
                            wifi_audio_voice_stop();
                            lvgl_lock(); kbd_screen_set_voice_active(false); lvgl_unlock();
                        }
                    }
                    break;
                }

                /* A: short=Enter/toggle (no more voice PTT on A) */
                if (key == KEY_A) {
                    if (!pressed) {
                        if (kbd_screen_is_enabled()) {
                            ble_hid_send_key(0, 0x28); /* A = Enter */
                        } else {
                            lvgl_lock(); kbd_screen_select(); lvgl_unlock();
                        }
                    }
                    break;
                }

                /* Other arrow keys (UP, DOWN, LEFT) — press only */
                if (!pressed) break;
                if (kbd_screen_is_enabled()) {
                    uint8_t hid = 0;
                    switch (key) {
                        case KEY_UP:    hid = 0x52; break;
                        case KEY_DOWN:  hid = 0x51; break;
                        case KEY_LEFT:  hid = 0x50; break;
                        default: break;
                    }
                    if (hid) ble_hid_send_key(0, hid);
                }

            } else if (app_manager_get_current_app() == APP_ID_MOUSE) {
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
                    /* Mouse ON: A=left click, UP/DOWN nav sensitivity */
                    if (key == KEY_A) { ble_hid_mouse_click(0x01); break; }
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
                    case KEY_START:
                        /* In sub-screen: back to list; in list: exit settings */
                        if (!settings_screen_back())
                            app_manager_return();
                        break;
                    default: break;
                }
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_COUNTDOWN) {
                static bool hold_start_t = false, hold_b_t = false;
                if (pressed) {
                    if (key == KEY_START) hold_start_t = true;
                    if (key == KEY_B) hold_b_t = true;
                } else {
                    if (key == KEY_START) hold_start_t = false;
                    if (key == KEY_B) hold_b_t = false;
                    if (key == KEY_A && countdown_screen_is_finished()) {
                        lvgl_lock();
                        countdown_screen_reset();
                        lvgl_unlock();
                    }
                }
                if (hold_start_t && hold_b_t) {
                    hold_start_t = hold_b_t = false;
                    lvgl_lock();
                    app_manager_return();
                    lvgl_unlock();
                }
            } else if (app_manager_get_current_app() == APP_ID_FORTUNE && pressed) {
                lvgl_lock();
                switch (key) {
                    case KEY_UP:    fortune_screen_navigate(-1); break;
                    case KEY_DOWN:  fortune_screen_navigate(1);  break;
                    case KEY_A:     fortune_screen_select();     break;
                    case KEY_B:
                    case KEY_START: app_manager_return(); break;
                    default: break;
                }
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_RECORDER && pressed) {
                lvgl_lock();
                switch (key) {
                    case KEY_UP:    liuren_screen_navigate(-1); break;
                    case KEY_DOWN:  liuren_screen_navigate(1);  break;
                    case KEY_A:     liuren_screen_calculate(); break;
                    case KEY_B:
                    case KEY_START: app_manager_return(); break;
                    default: break;
                }
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_CALIB) {
                if (!pressed && key == KEY_A) {
                    lvgl_lock(); calib_screen_trigger(); lvgl_unlock();
                }
                if (!pressed && (key == KEY_B || key == KEY_START)) {
                    lvgl_lock(); app_manager_return(); lvgl_unlock();
                }
            } else if (app_manager_get_current_app() == APP_ID_THEME && pressed) {
                lvgl_lock();
                if (key == KEY_UP)        theme_screen_navigate(-1);
                else if (key == KEY_DOWN)  theme_screen_navigate(1);
                else if (key == KEY_A)     theme_screen_select();
                else if (key == KEY_B || key == KEY_START) app_manager_return();
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_SPECTRUM && pressed) {
                lvgl_lock();
                if (key == KEY_UP)        spectrum_screen_navigate(-1);
                else if (key == KEY_DOWN)  spectrum_screen_navigate(1);
                else if (key == KEY_LEFT)  spectrum_screen_adjust(-1);
                else if (key == KEY_RIGHT) spectrum_screen_adjust(1);
                else if (key == KEY_A)     spectrum_screen_adjust(1);
                else if (key == KEY_B || key == KEY_START) app_manager_return();
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_IP_INPUT && pressed) {
                lvgl_lock();
                if (key == KEY_UP)        ip_input_navigate(0, 1);
                else if (key == KEY_DOWN)  ip_input_navigate(0, -1);
                else if (key == KEY_A)     ip_input_select();
                else if (key == KEY_B || key == KEY_START) { ip_input_cancel(); app_manager_return(); }
                lvgl_unlock();
            } else if (app_manager_get_current_app() == APP_ID_WIFI_SETUP && pressed) {
                if (key == KEY_A) {
                    /* Release LVGL lock before blocking BT deinit (~2s) */
                    lvgl_unlock();
                    ble_hid_deinit(); /* stop BLE to free 2.4GHz for WiFi AP */
                    wifi_manager_forget_ssids();
                    wifi_manager_stop_station();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    wifi_manager_start_config();
                    lvgl_lock();
                    app_manager_launch(APP_ID_WIFI_SETUP); /* refresh screen */
                    lvgl_unlock();
                } else if (key == KEY_B || key == KEY_START) {
                    lvgl_lock();
                    app_manager_return();
                    lvgl_unlock();
                }
            } else if (pressed && (key == KEY_B || key == KEY_START)) {
                lvgl_lock();
                app_manager_return();
                lvgl_unlock();
            }
            break;
    }
}

/* ================================================================
 * Power management task — 背光控制（5s间隔，暂不进入 Light Sleep）
 * ================================================================ */
static void pm_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (s_sleeping) continue;

        settings_t cfg;
        if (settings_load(&cfg) != ESP_OK || !cfg.sleep_enabled) continue;
        if (app_manager_get_current_app() == APP_ID_COUNTDOWN) continue;
        if (app_manager_get_current_app() == APP_ID_SPECTRUM) continue;

        TickType_t now = xTaskGetTickCount();
        if ((now - s_last_activity) <= pdMS_TO_TICKS(cfg.sleep_timeout_sec * 1000))
            continue;

        s_sleeping = true;
        bsp_lcd_backlight_set(0);
        ESP_LOGI(TAG, "Display off (timeout=%ds, light sleep TBD)", cfg.sleep_timeout_sec);
    }
}

/* ================================================================
 * Entry point
 * ================================================================ */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 Smart Handheld ===");

    /* Small delay to allow serial monitor to capture boot messages */
    vTaskDelay(pdMS_TO_TICKS(1000));

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

    /* 2b. Theme (NVS-backed) */
    theme_load();

    /* 3. SD card first (creates SPI bus at safe speed, LCD joins later) */
    ret = sd_card_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "SD card init failed (continuing): %s", esp_err_to_name(ret));

    /* 4. BSP: LCD joins existing SPI bus */
    bsp_board_init();
    bsp_lcd_backlight_set(cfg.brightness);

    /* 5. LVGL display driver */
    ret = ui_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: Display init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* 6. Audio - MAX98357 + MSM261 digital mic */
    ret = box_audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio init FAILED: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Audio init OK, playing beep...");
        box_audio_beep();
        ESP_LOGI(TAG, "Beep done");
        box_audio_diag();  /* 诊断：打印 I2S 麦克风数据 */
    }

    /* 7. Sensors (best-effort) */
    ret = aht20_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "AHT20 init failed (continuing)");
    ret = imu_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "IMU init failed (continuing)");
    } 
    /* IMU is on-demand — air mouse / fortune / countdown call imu_read() directly */

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

    /* 14. NES wrapper removed — games on separate partition (ota_0) */

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

    /* 16. Power management — sleep monitor */
    s_last_activity = xTaskGetTickCount();
    xTaskCreatePinnedToCore(pm_task, "pm", 4096, NULL, 1, NULL, 0);

    /* 17. App manager — shows launcher/home screen */
    app_manager_init();

    ESP_LOGI(TAG, "System ready. Idle loop.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
