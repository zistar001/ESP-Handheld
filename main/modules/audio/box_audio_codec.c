#include "box_audio_codec.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
 * New PCB (Rev 2) Audio Architecture:
 *   ┌─ I2S0 (RX) ──────────────────────┐
 *   │  BCLK=IO11, WS=IO12, DATA=IO10   │
 *   │  → MSM261 数字麦克风 (I2S)        │
 *   └────────────────────────────────────┘
 *   ┌─ I2S1 (TX) ──────────────────────┐
 *   │  BCLK=IO46, LRC=IO9, DIN=IO3     │
 *   │  → MAX98357 数字功放 (I2S入)      │
 *   └────────────────────────────────────┘
 *
 * 两个独立I2S控制器，各自独立时钟。无外部音频编解码芯片。
 * 音量控制由软件乘法实现（MAX98357无I2C音量控制）。
 */

static const char *TAG = "BOX_AUDIO";

#define AUDIO_BITS        16

static uint32_t s_sample_rate = 16000;

/* I2S0 TX → MAX98357 */
static i2s_chan_handle_t s_tx_chan = NULL;

/* I2S1 RX → 数字麦克风 */
static i2s_chan_handle_t s_rx_chan = NULL;

static uint8_t s_volume = 70;      /* 0-100, 软件音量 */
static float   s_mic_gain = 1.0f;  /* 麦克风增益系数 */
static bool    s_inited = false;

/* ================================================================
 *  I2S0 TX — MAX98357 数字功放
 * ================================================================ */
static esp_err_t init_i2s0_tx(uint32_t rate)
{
    i2s_chan_config_t chan_cfg = {
        .id = BSP_AMP_HOST,  /* I2S_NUM_0 */
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    /* TX only — rx_handle = NULL */
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL),
                        TAG, "i2s_new_channel(TX) failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   /* MAX98357 不需要 MCLK */
            .bclk = BSP_AMP_BCLK,      /* IO46 */
            .ws   = BSP_AMP_LRC,       /* IO9 */
            .dout = BSP_AMP_DIN,       /* IO3 */
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {false, false, false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_chan, &std_cfg),
                        TAG, "i2s_channel_init_std_mode(TX) failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_chan),
                        TAG, "i2s_channel_enable(TX) failed");

    /* MAX98357 上电稳定需要 ~20ms */
    vTaskDelay(pdMS_TO_TICKS(25));

    ESP_LOGI(TAG, "I2S0 TX: MAX98357 @ %lu Hz (BCLK=46, LRC=9, DIN=3)", rate);
    return ESP_OK;
}

/* ================================================================
 *  I2S1 RX — 数字麦克风
 * ================================================================ */
static esp_err_t init_i2s1_rx(uint32_t rate)
{
    i2s_chan_config_t chan_cfg = {
        .id = BSP_MIC_HOST,  /* I2S_NUM_1 */
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    /* RX only — tx_handle = NULL */
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, NULL, &s_rx_chan),
                        TAG, "i2s_new_channel(RX) failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   /* 数字麦克风不需要 MCLK */
            .bclk = BSP_MIC_BCLK,      /* IO11 */
            .ws   = BSP_MIC_WS,        /* IO12 */
            .dout = I2S_GPIO_UNUSED,
            .din  = BSP_MIC_DATA,      /* IO10 */
            .invert_flags = {false, false, false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx_chan, &std_cfg),
                        TAG, "i2s_channel_init_std_mode(RX) failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx_chan),
                        TAG, "i2s_channel_enable(RX) failed");

    ESP_LOGI(TAG, "I2S1 RX: Digital Mic @ %lu Hz (BCLK=11, WS=12, DATA=10)", rate);
    return ESP_OK;
}

/* ================================================================
 *  公共 API
 * ================================================================ */

esp_err_t box_audio_init(void)
{
    return box_audio_init_rate(16000);
}

esp_err_t box_audio_init_rate(uint32_t sample_rate)
{
    if (s_inited && s_sample_rate == sample_rate) return ESP_OK;

    /* 先清理旧资源 */
    if (s_inited) {
        ESP_LOGI(TAG, "Reconfiguring %lu -> %lu Hz", s_sample_rate, sample_rate);
        box_audio_stop();
    }

    s_sample_rate = sample_rate;

    ESP_RETURN_ON_ERROR(init_i2s0_tx(sample_rate), TAG, "I2S0 TX init failed");
    ESP_RETURN_ON_ERROR(init_i2s1_rx(sample_rate), TAG, "I2S1 RX init failed");

    ESP_LOGI(TAG, "Audio ready: MAX98357 + Digital Mic @ %lu Hz %d-bit", sample_rate, AUDIO_BITS);
    s_inited = true;
    return ESP_OK;
}

esp_err_t box_audio_read(int16_t *data, int samples)
{
    if (!s_inited || !s_rx_chan) return ESP_ERR_INVALID_STATE;

    /* 16-bit STEREO: 每帧2个int16_t, 取左右声道中信号大的那个 */
    int frames = samples;
    int16_t *stereo = (int16_t *)heap_caps_malloc(frames * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!stereo) stereo = (int16_t *)malloc(frames * 2 * sizeof(int16_t));
    if (!stereo) return ESP_ERR_NO_MEM;

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(s_rx_chan, stereo,
                                     frames * 2 * sizeof(int16_t), &bytes_read,
                                     portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "i2s_channel_read failed: %s", esp_err_to_name(ret));
        free(stereo);
        return ESP_FAIL;
    }

    int n = bytes_read / sizeof(int16_t);
    int out = 0;
    for (int i = 0; i < n - 1 && out < samples; i += 2) {
        int32_t l = stereo[i];
        int32_t r = stereo[i + 1];
        /* 取左右声道中信号较大的那个 — 适配不同 MSM261 变种 */
        int32_t v = (abs(l) > abs(r)) ? l : r;
        if (s_mic_gain != 1.0f) v = (int32_t)(v * s_mic_gain);
        data[out++] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
    free(stereo);

    if (out == 0) {
        ESP_LOGW(TAG, "box_audio_read: no data (n=%d)", n);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t box_audio_write(const int16_t *data, int samples)
{
    if (!s_inited || !s_tx_chan) return ESP_ERR_INVALID_STATE;

    /* I2S配置为32-bit，将16-bit数据扩展到32-bit（左对齐到MSB） */
    int32_t *buf32 = (int32_t *)heap_caps_malloc(samples * sizeof(int32_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf32) buf32 = (int32_t *)malloc(samples * sizeof(int32_t));
    if (!buf32) return ESP_ERR_NO_MEM;

    float gain = s_volume / 100.0f;
    for (int i = 0; i < samples; i++) {
        int32_t v = (int32_t)(data[i] * gain);
        buf32[i] = (v << 16);
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(s_tx_chan, buf32,
                                      samples * sizeof(int32_t), &bytes_written,
                                      portMAX_DELAY);
    free(buf32);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "i2s_channel_write failed: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

void box_audio_set_volume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    s_volume = vol;
}

uint8_t box_audio_get_volume(void)
{
    return s_volume;
}

void box_audio_set_mic_gain(uint8_t db)
{
    /* 将 0-37dB 映射为增益系数 — 对照旧硬件 ES7210 的 37dB ≈ 70x */
    float gain = 1.0f;
    if (db >= 37) gain = 70.0f;          /* 最大增益 */
    else if (db >= 30) gain = 35.0f;
    else if (db >= 20) gain = 15.0f;
    else if (db >= 10) gain = 5.0f;
    else if (db > 0) gain = 1.0f + (db * 0.5f);
    if (gain < 1.0f) gain = 1.0f;
    if (gain > 70.0f) gain = 70.0f;
    s_mic_gain = gain;
    ESP_LOGI(TAG, "mic_gain set: db=%d → gain=%.1fx", db, gain);
}

void box_audio_beep(void)
{
    if (!s_inited || !s_tx_chan) {
        ESP_LOGW(TAG, "beep skipped: inited=%d tx=%p", s_inited, s_tx_chan);
        return;
    }
#define BEEP_FREQ      880
#define BEEP_AMPLITUDE 28000  /* MAX98357 灵敏度有限，加大振幅使提示音更响 */
#define BEEP_DURATION  0.15f
    int samples = (int)(s_sample_rate * BEEP_DURATION);
    int16_t *buf = (int16_t *)heap_caps_malloc(samples * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) { ESP_LOGW(TAG, "beep skipped: OOM"); return; }
    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(sinf(2.0f * M_PI * BEEP_FREQ * i / s_sample_rate) * BEEP_AMPLITUDE);
    }
    esp_err_t r = box_audio_write(buf, samples);
    ESP_LOGI(TAG, "beep play: ret=%s", esp_err_to_name(r));
    heap_caps_free(buf);
}

/* 诊断：读取一帧 I2S RX 数据并打印统计 */
void box_audio_diag(void)
{
    if (!s_inited || !s_rx_chan) {
        ESP_LOGW(TAG, "diag skipped: inited=%d rx=%p", s_inited, s_rx_chan);
        return;
    }
    int16_t buf[64];  /* 32 stereo frames */
    size_t br = 0;
    esp_err_t ret = i2s_channel_read(s_rx_chan, buf, sizeof(buf), &br, pdMS_TO_TICKS(100));
    int n = br / sizeof(int16_t);
    int16_t vmin = 32767, vmax = -32768;
    for (int i = 0; i < n; i++) {
        if (buf[i] < vmin) vmin = buf[i];
        if (buf[i] > vmax) vmax = buf[i];
    }
    ESP_LOGI(TAG, "diag: ret=%s br=%d n=%d L=[%d %d %d %d] R=[%d %d %d %d] range=[%d..%d]",
             esp_err_to_name(ret), (int)br, n,
             n>0?buf[0]:0, n>2?buf[2]:0, n>4?buf[4]:0, n>6?buf[6]:0,
             n>1?buf[1]:0, n>3?buf[3]:0, n>5?buf[5]:0, n>7?buf[7]:0,
             vmin, vmax);
}

void box_audio_stop(void)
{
    if (s_tx_chan) {
        i2s_channel_disable(s_tx_chan);
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
    }
    if (s_rx_chan) {
        i2s_channel_disable(s_rx_chan);
        i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
    }
    s_inited = false;
    ESP_LOGI(TAG, "Audio stopped");
}

bool box_audio_is_inited(void)
{
    return s_inited;
}

uint32_t box_audio_get_sample_rate(void)
{
    return s_sample_rate;
}

i2s_chan_handle_t box_audio_get_tx_channel(void)
{
    return s_tx_chan;
}
