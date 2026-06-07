#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Box Audio Codec ? ES8311 (DAC) + ES7210 (ADC) on shared I2S_NUM_0.
 *
 * Matches Xiaozhi BoxAudioCodec pattern:
 *   - I2S TX: standard mode -> ES8311 output
 *   - I2S RX: TDM mode (4 slots) <- ES7210 mic input
 *   - Shared MCLK=40, BCLK=39, WS=45
 *   - Audio codec I2C: SDA=38, SCL=47 (separate from sensor bus)
 */

/* Initialize at default 16 kHz (voice/chat) */
esp_err_t box_audio_init(void);

/* Initialize (or reconfigure) at a specific sample rate.
 * If already initialized at a different rate, reconfigures I2S clock. */
esp_err_t box_audio_init_rate(uint32_t sample_rate);

esp_err_t box_audio_read(int16_t *data, int samples);
esp_err_t box_audio_write(const int16_t *data, int samples);
void     box_audio_set_volume(uint8_t vol);    /* 0-100 */
uint8_t  box_audio_get_volume(void);
void     box_audio_set_mic_gain(uint8_t db);   /* 0-37 dB */
void     box_audio_stop(void);
bool     box_audio_is_inited(void);
uint32_t box_audio_get_sample_rate(void);

#include "driver/i2s_std.h"

/* Get the raw I2S TX channel for direct writes (bypasses codec layer) */
i2s_chan_handle_t box_audio_get_tx_channel(void);
