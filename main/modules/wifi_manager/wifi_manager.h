#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

esp_err_t wifi_manager_init(void);
void wifi_manager_start_station(void);
void wifi_manager_stop_station(void);
bool wifi_manager_wait_connected(TickType_t timeout);
bool wifi_manager_is_connected(void);
int wifi_manager_get_rssi(void);
const char *wifi_manager_get_ip(void);
const char *wifi_manager_get_ssid(void);
void wifi_manager_forget_ssids(void);

/* Config AP / provisioning mode */
void wifi_manager_start_config(void);
void wifi_manager_stop_config(void);
bool wifi_manager_is_config_mode(void);
bool wifi_manager_has_saved_ssid(void);
const char *wifi_manager_get_ap_ssid(void);
const char *wifi_manager_get_ap_url(void);
