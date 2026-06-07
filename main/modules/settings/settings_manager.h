#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t volume;       // 0-100
    uint8_t brightness;   // 0-100
    uint8_t mic_gain;     // 0-100
    float air_mouse_sens; // 0.1-5.0
    char wifi_ssid[32];
    char wifi_pass[64];
    bool sleep_enabled;
    uint16_t sleep_timeout_sec;
    uint8_t key_map[7];
} settings_t;

esp_err_t settings_init(void);
esp_err_t settings_load(settings_t *s);
esp_err_t settings_save(const settings_t *s);
esp_err_t settings_reset(void);
void settings_print(const settings_t *s);
