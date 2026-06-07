#pragma once
#include "esp_err.h"
#include <stdbool.h>

/**
 * XiaoZhi AI voice assistant wrapper.
 *
 * Full source: D:\code\XIAOZHI_AI\xiaozhi-esp32-main\main
 *
 * Integration when hardware is available:
 * 1. Copy audio_codec, audio_service, application.cc from source
 * 2. Adapt ES8311+ES7210 I2S config in audio_codec.cc
 * 3. Set WiFi credentials for WebSocket connection
 * 4. Register xiaozhi_start/stop in app_manager
 */
esp_err_t xiaozhi_init(void);
esp_err_t xiaozhi_start(void);
void xiaozhi_stop(void);
bool xiaozhi_is_running(void);
