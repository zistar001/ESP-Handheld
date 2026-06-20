#pragma once
#include <stdint.h>

#define FFT_N       256
#define SPECTRUM_BARS 16

typedef struct {
    uint16_t bands[SPECTRUM_BARS];
} spectrum_result_t;

/* 可调参数 */
typedef struct {
    float  noise_threshold;   /* 静音门限，默认 0.01 (0.001~0.1) */
    uint8_t smoothing;        /* 平滑度 0-9 (0=极快, 9=极慢), 默认 7 */
    uint8_t mic_gain;         /* 麦克风增益 0-37 dB, 默认 37 */
} spectrum_params_t;

void spectrum_init(void);
void spectrum_analyze(const int16_t *samples, int count, spectrum_result_t *out);

/* 参数存取 — 频谱界面用 UP/DOWN/A 调节 */
const spectrum_params_t *spectrum_get_params(void);
void spectrum_set_noise_threshold(float v);
void spectrum_set_smoothing(uint8_t v);
void spectrum_set_mic_gain_db(uint8_t db);
