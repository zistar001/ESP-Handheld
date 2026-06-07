/* WiFi provisioning moved to esp-wifi-connect component via wifi_bridge */
#include "wifi_provisioning.h"
#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "WIFI_PROV";

esp_err_t wifi_provisioning_init(void) { return wifi_manager_init(); }
bool wifi_provisioning_is_active(void) { return wifi_manager_is_config_mode(); }
void wifi_provisioning_start(void) { wifi_manager_start_config(); }
void wifi_provisioning_stop(void) { wifi_manager_stop_config(); }
