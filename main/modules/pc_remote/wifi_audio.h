#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_audio_connect(const char *ssid, const char *pass);
void wifi_audio_disconnect(void);
bool wifi_audio_is_connected(void);
esp_err_t wifi_audio_start(const char *pc_ip, uint16_t port);
void wifi_audio_stop(void);
bool wifi_audio_is_streaming(void);
