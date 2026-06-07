#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * ES7210 Audio ADC driver (C API) — dual microphone input.
 *
 * I2S RX on I2S_NUM_1 (standard mode, 16kHz 16bit stereo).
 * BCK=38, WS=37, DIN=39. Dual mic on TDM, stereo output (L=R mic1, R=R mic2).
 */

esp_err_t es7210_init(void);
esp_err_t es7210_read(int16_t *data, int samples);
void     es7210_set_gain(uint8_t db);
void     es7210_stop(void);
bool     es7210_is_initialized(void);
