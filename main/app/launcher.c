#include "launcher.h"
#include "app_manager.h"
#include "ui/screens/home_screen.h"
#include "ui/display_driver.h"
#include "modules/time_sync/time_sync.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "modules/power/battery_monitor.h"
#include "modules/sensor/aht20_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LAUNCHER";

static void launcher_update_task(void *arg) {
    (void)arg;
    int tick = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        tick++;

        /* Only update when launcher screen is active — home screen objects exist */
        if (app_manager_get_state() != APP_STATE_LAUNCHER) continue;

        lvgl_lock();
        /* Time — update every second */
        home_screen_update_time(time_sync_get_time_string(),
                                time_sync_get_date_string());

        /* WiFi — check every 5s */
        if (tick % 5 == 0) {
            home_screen_update_wifi(wifi_manager_is_connected());
        }

        /* Battery — check every 5s */
        if (tick % 5 == 0) {
            int pct = battery_monitor_get_percentage();
            bool charging = battery_monitor_is_charging();
            home_screen_update_battery(pct, charging);
        }

        /* Indoor AHT20 — check every 10s */
        if (tick % 10 == 0) {
            float t = 0, h = 0;
            if (aht20_read(&t, &h) == ESP_OK) {
                home_screen_update_indoor(t, h);
            }
        }
        lvgl_unlock();
    }
}

void launcher_enter(void)
{
    ESP_LOGI(TAG, "enter");
    home_screen_create();
    app_manager_set_state(APP_STATE_LAUNCHER);

    /* Start periodic update task (only once) */
    static bool task_started = false;
    if (!task_started) {
        task_started = true;
        xTaskCreatePinnedToCore(launcher_update_task, "launcher_upd",
                                4096, NULL, 1, NULL, 0);
    }
}
