#include "spectrum_analyzer.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════
 * 固定点 Radix-2 DIT FFT
 *
 * twiddle: W_N^k = cos(2πk/N) - j·sin(2πk/N)
 * 预计算成 Q15 定点格式 (±32767)
 * ══════════════════════════════════════════════════════════════════ */

#define Q15_MAX 32767

static int16_t twiddle_cos[FFT_N / 2];   /* cos 表，Q15 */
static int16_t twiddle_sin[FFT_N / 2];   /* sin 表，Q15（取负后即 -sin） */
static int32_t fft_buf[FFT_N * 2];       /* 实部/虚部交错 */
static bool s_inited = false;

void spectrum_init(void) {
    if (s_inited) return;
    for (int k = 0; k < FFT_N / 2; k++) {
        float angle = -2.0f * M_PI * k / FFT_N;  /* -j 方向，即正向 FFT */
        twiddle_cos[k] = (int16_t)(cosf(angle) * Q15_MAX);
        twiddle_sin[k] = (int16_t)(sinf(angle) * Q15_MAX);
    }
    s_inited = true;
}

/* ── 位反转 ── */
static void bit_reverse(int32_t *data, int n) {
    for (int i = 0, j = 0; i < n; i++) {
        if (i < j) {
            int32_t tr = data[i * 2], ti = data[i * 2 + 1];
            data[i * 2] = data[j * 2];
            data[i * 2 + 1] = data[j * 2 + 1];
            data[j * 2] = tr;
            data[j * 2 + 1] = ti;
        }
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
    }
}

/* ── Q15 复数乘法： (ar+j·ai)*(br+j·bi) ── */
static inline void cmul_q15(int16_t ar, int16_t ai, int16_t br, int16_t bi,
                             int32_t *rr, int32_t *ri) {
    *rr = ((int32_t)ar * br - (int32_t)ai * bi) >> 15;
    *ri = ((int32_t)ar * bi + (int32_t)ai * br) >> 15;
}

/* ── FFT 主循环 ── */
static void fft_run(int32_t *data, int n) {
    bit_reverse(data, n);

    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int step = FFT_N / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                int tw = j * step;
                int16_t wr = twiddle_cos[tw];
                int16_t wi = twiddle_sin[tw];

                int idx_u = (i + j) * 2;
                int idx_l = (i + j + half) * 2;

                int32_t tr, ti;
                cmul_q15(wr, wi,
                         (int16_t)(data[idx_l] >> 8),   /* 缩放到 Q15 近似 */
                         (int16_t)(data[idx_l + 1] >> 8),
                         &tr, &ti);

                int32_t ur = data[idx_u];
                int32_t ui = data[idx_u + 1];
                data[idx_l]     = ur - tr;
                data[idx_l + 1] = ui - ti;
                data[idx_u]     = ur + tr;
                data[idx_u + 1] = ui + ti;
            }
        }
    }
}

/* ── Hann 窗 ── */
static void apply_hann(int32_t *data, int n) {
    for (int i = 0; i < n; i++) {
        float w = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (n - 1)));
        data[i * 2] = (int32_t)(data[i * 2] * w);
        /* 虚部初始为 0，不变 */
    }
}

/* ── 幅度计算 + 对数频段归并 → 16 柱 ── */
static void bin_to_bars(const int32_t *data, int n, spectrum_result_t *out) {
    /* 每柱代表一个对数间距的频段 */
    static const int bar_bins[SPECTRUM_BARS + 1] = {
        0,  1,  2,  3,  4,  6,  8,  10,    /* 0-7:  0Hz ~ 500Hz */
        13, 16, 20, 25, 32, 40, 50, 64,    /* 8-15: 500Hz ~ 4kHz */
        128                                  /* 终点: n/2 = 128 bins */
    };

    for (int b = 0; b < SPECTRUM_BARS; b++) {
        int32_t sum = 0;
        int start = bar_bins[b];
        int end   = bar_bins[b + 1];
        /* 只取 n/2 以内的正频率 */
        if (start >= n / 2) break;
        if (end > n / 2) end = n / 2;
        for (int k = start; k < end; k++) {
            int32_t r = data[k * 2];
            int32_t i = data[k * 2 + 1];
            /* 幅度 ≈ sqrt(r² + i²)，用近似：|r| + |i| */
            int32_t mag = (r < 0 ? -r : r) + (i < 0 ? -i : i);
            sum += mag;
        }
        int count = end - start;
        if (count > 0) sum /= count;
        /* 缩放到 0-4095 */
        sum >>= 8;  /* FFT 内部有 ×256 的放大，这里缩回 */
        if (sum > 4095) sum = 4095;
        if (sum < 0) sum = 0;
        out->bands[b] = (uint16_t)sum;
    }
}

/* ── 公共接口 ── */
void spectrum_analyze(const int16_t *samples, int count, spectrum_result_t *out) {
    if (!s_inited) spectrum_init();

    /* 填充 FFT 缓冲区（截断或零填充到 FFT_N） */
    int n = (count < FFT_N) ? count : FFT_N;
    for (int i = 0; i < n; i++) {
        fft_buf[i * 2]     = (int32_t)samples[i] * 256;  /* 放大提高精度 */
        fft_buf[i * 2 + 1] = 0;
    }
    for (int i = n; i < FFT_N; i++) {
        fft_buf[i * 2] = 0;
        fft_buf[i * 2 + 1] = 0;
    }

    apply_hann(fft_buf, FFT_N);
    fft_run(fft_buf, FFT_N);
    bin_to_bars(fft_buf, FFT_N, out);
}
