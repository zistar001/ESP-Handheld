#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * ES8311 audio codec driver (C API).
 *
 * Reference: Xiaozhi AI project's Es8311AudioCodec C++ class.
 * Uses esp_codec_dev component for hardware abstraction.
 */

esp_err_t es8311_init(void);
void     es8311_set_volume(uint8_t vol);       /* 0–100 */
uint8_t  es8311_get_volume(void);
esp_err_t es8311_write(const int16_t *data, int samples);
void     es8311_stop(void);
bool     es8311_is_initialized(void);
