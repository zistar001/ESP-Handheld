#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "esp_err.h"

typedef enum { APP_STATE_LAUNCHER, APP_STATE_MENU, APP_STATE_RUNNING } app_state_t;
typedef enum { APP_ID_NONE=-1, APP_ID_NES, APP_ID_KEYBOARD, APP_ID_MOUSE, APP_ID_COUNTDOWN, APP_ID_FORTUNE, APP_ID_RECORDER, APP_ID_SETTINGS, APP_ID_ABOUT, APP_ID_WIFI_SETUP, APP_ID_IP_INPUT, APP_ID_MAX } app_id_t;

typedef struct {
    app_id_t id;
    const char *name;
    const char *icon;
} app_entry_t;

void app_manager_init(void);
void app_manager_set_state(app_state_t s);
app_state_t app_manager_get_state(void);
app_id_t app_manager_get_current_app(void);
esp_err_t app_manager_launch(app_id_t id);
void app_manager_return(void);
void app_manager_wifi_action(void);  /* trigger WiFi reset/setup from key */
const app_entry_t *app_manager_get_apps(void);
int app_manager_get_app_count(void);

#endif
