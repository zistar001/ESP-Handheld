#include "nes_wrapper.h"
#include "nes_game.h"
#include "ui/display_driver.h"
#include "bsp/key_driver.h"
#include "nes_port.h"
#include "app/app_manager.h"
#include "modules/pc_remote/ble_hid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "NES_WRAP";

static void on_game_exit(void) {
    ESP_LOGI(TAG, "Game exited, restoring LVGL");
    ui_display_set_nes_active(false);
    /* Delay to let pending SPI DMA transfers complete before BLE reclaims DMA */
    vTaskDelay(pdMS_TO_TICKS(200));
    ble_hid_init();
    app_manager_return();
}

void nes_wrapper_init(void) {
    nes_game_set_exit_callback(on_game_exit);
    esp_err_t ret = nes_game_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nes_game_init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "NES wrapper ready");
    }
}

void nes_start(const char *rom_path) {
    if (nes_game_is_running()) {
        ESP_LOGW(TAG, "Game already running, stop first");
        return;
    }
    ESP_LOGI(TAG, "Starting game: %s", rom_path);
    ble_hid_deinit();
    /* Retry NES init if it failed at boot (BLE was consuming DMA) */
    nes_game_init();
    ui_display_set_nes_active(true);
    nes_game_start(rom_path);
}

void nes_stop(void) {
    ESP_LOGI(TAG, "Stopping game...");
    nes_game_stop();
    ui_display_set_nes_active(false);
    ESP_LOGI(TAG, "Game stopped, LVGL restored");
}

bool nes_is_running(void) {
    return nes_game_is_running();
}

void nes_key_event(key_id_t key, bool pressed) {
    switch (key) {
        case KEY_UP:    gamepad_p1.JOY_UP    = pressed; break;
        case KEY_DOWN:  gamepad_p1.JOY_DOWN  = pressed; break;
        case KEY_LEFT:  gamepad_p1.JOY_LEFT  = pressed; break;
        case KEY_RIGHT: gamepad_p1.JOY_RIGHT = pressed; break;
        case KEY_A:     gamepad_p1.JOY_A     = pressed; break;
        case KEY_B:     gamepad_p1.JOY_B     = pressed; break;
        case KEY_START: gamepad_p1.JOY_START = pressed; break;
        default: break;
    }
}
