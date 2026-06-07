#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_bridge_init(const char *ssid_prefix, const char *lang);
bool wifi_bridge_is_connected(void);
void wifi_bridge_start_station(void);
void wifi_bridge_stop_station(void);
int wifi_bridge_get_rssi(void);
const char *wifi_bridge_get_ip(void);
const char *wifi_bridge_get_ssid(void);
void wifi_bridge_forget_ssids(void);
bool wifi_bridge_wait_connected(TickType_t timeout);
bool wifi_bridge_has_saved_ssid(void);
void wifi_bridge_add_ssid(const char *ssid, const char *password);
void wifi_bridge_start_config(void);
void wifi_bridge_stop_config(void);
bool wifi_bridge_is_config_mode(void);
const char *wifi_bridge_get_ap_ssid(void);
const char *wifi_bridge_get_ap_url(void);

#ifdef __cplusplus
}
#endif
