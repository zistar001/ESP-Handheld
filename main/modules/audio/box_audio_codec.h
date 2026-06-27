#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * New PCB (Rev 2) Audio:
 *   - I2S0 TX → MAX98357 数字功放 (BCLK=IO46, LRC=IO9, DIN=IO3)
 *   - I2S1 RX ← 数字麦克风 (BCLK=IO11, WS=IO12, DATA=IO10)
 *   - 无外部音频编解码芯片，无I2C控制
 *   - 音量/增益由软件控制
 */

esp_err_t box_audio_init(void);
esp_err_t box_audio_init_rate(uint32_t sample_rate);
esp_err_t box_audio_read(int16_t *data, int samples);
esp_err_t box_audio_write(const int16_t *data, int samples);
void     box_audio_set_volume(uint8_t vol);    /* 0-100 */
uint8_t  box_audio_get_volume(void);
void     box_audio_beep(void);
void     box_audio_set_mic_gain(uint8_t db);   /* 0-37 dB → 映射为软件增益 */
void     box_audio_stop(void);
bool     box_audio_is_inited(void);
uint32_t box_audio_get_sample_rate(void);
void     box_audio_diag(void);  /* 诊断：打印 I2S RX 数据统计 */

#include "driver/i2s_std.h"
i2s_chan_handle_t box_audio_get_tx_channel(void);
