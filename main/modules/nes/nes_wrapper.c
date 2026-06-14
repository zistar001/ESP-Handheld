#include "nes_wrapper.h"
#include "nes_game.h"
#include "app/app_manager.h"
#include "ui/display_driver.h"
#include "bsp/key_driver.h"
#include "nes_port.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "NES_WRAP";

/* Flag set by video_task callback, consumed by key_handler on next key event.
 * This avoids LVGL operations inside video_task context entirely. */
static volatile bool s_game_exit_pending = false;

static void on_game_exit(void) {
    /* Called from video_task context (Core 0) — do NOT touch LVGL here.
     * Just flag it; the key handler will pick it up on the next key event. */
    s_game_exit_pending = true;
}

/* Called at the top of key_handler — runs in key_task context (Core 0, prio 5),
 * safe for LVGL operations. Returns true if exit was handled. */
bool nes_wrapper_check_exit(void) {
    if (!s_game_exit_pending) return false;
    s_game_exit_pending = false;
    ESP_LOGI(TAG, "Game over, returning to menu...");
    ui_display_set_nes_active(false);
    app_manager_return();
    return true;
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
    ESP_LOGI(TAG, "=== DBG nes_start: %s ===", rom_path);
    /* Re-register exit callback (it's cleared after each use) */
    nes_game_set_exit_callback(on_game_exit);
    ui_display_set_nes_active(true);
    ESP_LOGI(TAG, "=== DBG nes_start: calling nes_game_start ===");
    nes_game_start(rom_path);
    ESP_LOGI(TAG, "=== DBG nes_start: returned ===");
}

void nes_stop(void) {
    if (!nes_game_is_running()) return;
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
