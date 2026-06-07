#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

/* BSP */
#include "bsp_board.h"
#include "bsp/key_driver.h"
#include "bsp/sd_card.h"

/* UI */
#include "ui/display_driver.h"

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
 * Key handler — routes based on app_manager state
 * ================================================================ */
static void key_handler(key_id_t key, bool pressed) {
    app_state_t state = app_manager_get_state();

    switch (state) {
        case APP_STATE_LAUNCHER:
            if (pressed && key == KEY_START) {
                menu_enter();
            }
            break;

        case APP_STATE_MENU:
            if (pressed) {
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
            }
            break;

        case APP_STATE_RUNNING:
            if (app_manager_get_current_app() == APP_ID_NES) {
                /* NES: forward ALL key events (press + release) */
                rom_browser_key(key, pressed);
            } else if (pressed && (key == KEY_B || key == KEY_START)) {
                /* Other apps: B or START goes back */
                app_manager_return();
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
    settings_t cfg;
    settings_load(&cfg);
    ESP_LOGI(TAG, "Settings loaded: vol=%d bright=%d", cfg.volume, cfg.brightness);

    /* 3. SD card first (creates SPI bus, LCD joins later — retro-go approach) */
    sd_card_init();

    /* 4. BSP: LCD + I2C */
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
    if (ret != ESP_OK) ESP_LOGW(TAG, "IMU init failed (continuing)");

    /* 8. Battery monitor (may fail due to ADC conflict) */
    ret = battery_monitor_init();
    if (ret != ESP_OK) ESP_LOGW(TAG, "Battery monitor init failed (continuing)");

    /* 9. Apply settings to hardware */
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
    xTaskCreatePinnedToCore(key_driver_scan_task, "key", 4096, NULL, 5, NULL, 0);

    /* 16. App manager — shows launcher/home screen */
    app_manager_init();

    ESP_LOGI(TAG, "System ready. Idle loop.");

    /* Idle: keep main task alive */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
