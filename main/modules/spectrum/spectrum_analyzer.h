#pragma once
#include <stdint.h>

#define FFT_N       256         /* FFT 点数（必须是 2 的幂） */
#define SPECTRUM_BARS 16        /* 显示柱数 */
#define SPECTRUM_SAMPLE_RATE 16000

/* 频谱分析结果：16 个频段的幅度 (0-4095) */
typedef struct {
    uint16_t bands[SPECTRUM_BARS];
} spectrum_result_t;

/* 初始化（不需要硬件，只准备 FFT 查找表） */
void spectrum_init(void);

/* 从 int16_t 音频缓冲执行一次 FFT 分析 */
void spectrum_analyze(const int16_t *samples, int count, spectrum_result_t *out);
