#include "launcher.h"
#include "app_manager.h"
#include "ui/screens/home_screen.h"
#include "esp_log.h"

static const char *TAG = "LAUNCHER";

void launcher_enter(void)
{
    ESP_LOGI(TAG, "enter");
    home_screen_create();
    app_manager_set_state(APP_STATE_LAUNCHER);
}
