#include "menu.h"
#include "app_manager.h"
#include "ui/screens/menu_screen.h"
#include "ui/components/status_bar.h"
#include "esp_log.h"

static const char *TAG = "MENU";

void menu_enter(void)
{
    ESP_LOGI(TAG, "enter");
    menu_screen_create();
    app_manager_set_state(APP_STATE_MENU);
}

void menu_navigate(int dx, int dy)
{
    menu_screen_navigate(dx, dy);
}

void menu_select(void)
{
    menu_screen_select();
}
