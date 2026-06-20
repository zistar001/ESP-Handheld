#include "spectrum_analyzer.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════
 * float32 FFT — 标准 Radix-2 DIT，无定点缩放问题
 * ══════════════════════════════════════════════════════════════════ */

static float twid_cos[FFT_N / 2];
static float twid_sin[FFT_N / 2];
static float fft_real[FFT_N];
static float fft_imag[FFT_N];
static bool s_inited = false;

/* 默认参数 */
static spectrum_params_t s_params = {
    .noise_threshold = 0.01f,
    .smoothing = 7,
    .mic_gain = 37,
};

const spectrum_params_t *spectrum_get_params(void) { return &s_params; }
void spectrum_set_noise_threshold(float v) { if (v >= 0.001f && v <= 0.1f) s_params.noise_threshold = v; }
void spectrum_set_smoothing(uint8_t v) { if (v <= 9) s_params.smoothing = v; }
void spectrum_set_mic_gain_db(uint8_t db) { if (db <= 60) s_params.mic_gain = db; }

void spectrum_init(void) {
    if (s_inited) return;
    for (int k = 0; k < FFT_N / 2; k++) {
        float a = -2.0f * (float)M_PI * k / FFT_N;
        twid_cos[k] = cosf(a);
        twid_sin[k] = sinf(a);
    }
    s_inited = true;
}

/* ── 位反转 ── */
static void bit_reverse(float *re, float *im, int n) {
    for (int i = 0, j = 0; i < n; i++) {
        if (i < j) {
            float tr = re[i], ti = im[i];
            re[i] = re[j]; im[i] = im[j];
            re[j] = tr;    im[j] = ti;
        }
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
    }
}

/* ── 标准 Cooley-Tukey 蝶形 ── */
static void fft_run(float *re, float *im, int n) {
    bit_reverse(re, im, n);

    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int step = FFT_N / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                int tw = j * step;
                float wr = twid_cos[tw];
                float wi = twid_sin[tw];

                int u = i + j;
                int l = i + j + half;

                float tr = wr * re[l] - wi * im[l];
                float ti = wr * im[l] + wi * re[l];

                re[l] = re[u] - tr;
                im[l] = im[u] - ti;
                re[u] = re[u] + tr;
                im[u] = im[u] + ti;
            }
        }
    }
}

/* ── Hann 窗 ── */
static void apply_hann(float *buf, int n) {
    for (int i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (n - 1)));
        buf[i] *= w;
    }
}

/* ── 线性幅度 + 绝对门限 + 等宽频段分组 ──
 * 参考 esp-dsp: mag = (re² + im²) / N, 再用 dB */
static void bin_to_bars(int n, spectrum_result_t *out) {
    /* 计算线性功率谱 / N（参考 esp-dsp 做法） */
    static float pwr[FFT_N / 2];
    float pwr_max = 0;
    float scale = 1.0f / (float)n;

    for (int k = 0; k < n / 2; k++) {
        float m = (fft_real[k] * fft_real[k] + fft_imag[k] * fft_imag[k]) * scale;
        pwr[k] = m;
        if (m > pwr_max) pwr_max = m;
    }

    /* 绝对门限：可调参数 noise_threshold */
    if (pwr_max < s_params.noise_threshold) {
        memset(out->bands, 0, sizeof(out->bands));
        return;
    }

    /* 对数频段分组：低频窄、高频宽，匹配人耳听觉
     * 128 bins @ 16kHz → 每个 bin = 62.5Hz
     * bins:  0=DC,  2=125Hz,  5=312Hz, ... 115=7.2kHz */
    static const int bar_bins[SPECTRUM_BARS + 1] = {
        0,  2,  3,  4,  5,  6,  8, 10,
       13, 17, 22, 29, 38, 50, 66, 87,
       128
    };
    int usable = n / 2;

    for (int b = 0; b < SPECTRUM_BARS; b++) {
        float peak = 0;
        int start = bar_bins[b];
        int end   = bar_bins[b + 1];
        if (start >= usable) break;
        if (end > usable) end = usable;
        for (int k = start; k < end; k++) {
            if (pwr[k] > peak) peak = pwr[k];
        }
        /* 映射：pwr_max → 3000, 线性映射 */
        uint16_t u = (uint16_t)(peak / pwr_max * 3000.0f);
        if (u > 3000) u = 3000;
        out->bands[b] = u;
    }
}

/* ── 公共接口 ── */
void spectrum_analyze(const int16_t *samples, int count, spectrum_result_t *out) {
    if (!s_inited) spectrum_init();

    /* int16 → float32, 去直流分量, 归一化到 [-1, 1] */
    int n = (count < FFT_N) ? count : FFT_N;
    float dc = 0;
    for (int i = 0; i < n; i++) dc += (float)samples[i];
    dc /= (float)n;
    for (int i = 0; i < n; i++) {
        fft_real[i] = ((float)samples[i] - dc) / 32768.0f;
        fft_imag[i] = 0;
    }
    for (int i = n; i < FFT_N; i++) {
        fft_real[i] = 0;
        fft_imag[i] = 0;
    }

    apply_hann(fft_real, FFT_N);
    fft_run(fft_real, fft_imag, FFT_N);
    bin_to_bars(FFT_N, out);
}
