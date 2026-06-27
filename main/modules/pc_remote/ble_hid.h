#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t ble_hid_init(void);
esp_err_t ble_hid_send_key(uint8_t modifier, uint8_t key);
void     ble_hid_press_key(uint8_t modifier, uint8_t key);
void     ble_hid_send_combo(uint8_t modifier, uint8_t key);
esp_err_t ble_hid_send_modkey(uint8_t modifier, uint8_t key);
esp_err_t ble_hid_send_mouse(uint8_t buttons, int8_t dx, int8_t dy);
void     ble_hid_mouse_click(uint8_t buttons);
void     ble_hid_release_all(void);
esp_err_t ble_hid_deinit(void);
bool ble_hid_is_connected(void);
bool ble_hid_is_initialized(void);
const char *ble_hid_get_passkey(void);
