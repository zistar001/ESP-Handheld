#include "zistar_audio_codec.h"
#include <esp_log.h>
#include <driver/i2s_std.h>

static const char TAG[] = "ZistarAudioCodec";

ZistarAudioCodec::ZistarAudioCodec(int input_sample_rate, int output_sample_rate,
    gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
    gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data,
    gpio_num_t spkr_enable) {
    duplex_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_channels_ = 1;
    output_channels_ = 1;
    spkr_en_pin_ = spkr_enable;
    input_gain_ = 37;  /* 最大麦克风增益 (37dB ≈ 70x, 匹配 box_audio_codec) */

    /* MSM261 mic on I2S0 (RX) — matches known working T-Circle-S3 config */
    i2s_chan_config_t mic_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    mic_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&mic_cfg, NULL, &rx_handle_));

    /* MAX98357 speaker on I2S1 (TX) */
    i2s_chan_config_t spkr_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    spkr_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&spkr_cfg, &tx_handle_, NULL));

    /* Mic config — 16-bit STEREO (匹配 T-Circle-S3 MSM261 配置) */
    i2s_std_config_t mic_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)input_sample_rate_),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = mic_bclk,
            .ws = mic_ws,
            .dout = I2S_GPIO_UNUSED,
            .din = mic_data,
            .invert_flags = {false, false, false},
        },
    };

    /* Speaker config — standard I2S 32-bit mono (matches NoAudioCodec) */
    i2s_std_config_t spkr_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)output_sample_rate_),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = spkr_bclk,
            .ws = spkr_lrclk,
            .dout = spkr_data,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {false, false, false},
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &mic_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &spkr_config));
    ESP_LOGI(TAG, "ZistarAudioCodec: mic on I2S0, spkr on I2S1");

    /* MAX98357 SD_MODE pin — pull high to enable */
    if (spkr_en_pin_ != GPIO_NUM_NC) {
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << spkr_en_pin_,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
        gpio_set_level(spkr_en_pin_, 1);
    }
}

ZistarAudioCodec::~ZistarAudioCodec() {
    if (rx_handle_) { i2s_channel_disable(rx_handle_); i2s_del_channel(rx_handle_); }
    if (tx_handle_) { i2s_channel_disable(tx_handle_); i2s_del_channel(tx_handle_); }
}

void ZistarAudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
}

void ZistarAudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == input_enabled_) return;
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
    }
    AudioCodec::EnableInput(enable);
}

void ZistarAudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == output_enabled_) return;
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
    }
    AudioCodec::EnableOutput(enable);
}

int ZistarAudioCodec::Read(int16_t* dest, int samples) {
    if (!input_enabled_) return 0;
    /* 16-bit STEREO I2S: 每帧2个int16_t (L+R), 取左右中信号大的那个 */
    int frames = samples;
    int16_t* stereo = (int16_t*)malloc(frames * 2 * sizeof(int16_t));
    if (!stereo) return 0;

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(rx_handle_, stereo,
                                     frames * 2 * sizeof(int16_t), &bytes_read, 200);
    if (ret != ESP_OK) { free(stereo); return 0; }
    int n = bytes_read / sizeof(int16_t);

    /* 增益映射：match box_audio_codec (37dB ≈ 70x) */
    float gain;
    if (input_gain_ >= 37) gain = 70.0f;
    else if (input_gain_ >= 30) gain = 35.0f;
    else if (input_gain_ >= 20) gain = 15.0f;
    else if (input_gain_ >= 10) gain = 5.0f;
    else gain = 1.0f + (input_gain_ > 0 ? input_gain_ * 0.5f : 0.0f);

    int out = 0;
    for (int i = 0; i < n - 1 && out < samples; i += 2) {
        int32_t l = stereo[i];
        int32_t r = stereo[i + 1];
        int32_t v = (abs(l) > abs(r)) ? l : r;  /* 取信号强的声道 */
        v = (int32_t)(v * gain);
        dest[out++] = (v > INT16_MAX) ? INT16_MAX : (v < -INT16_MAX) ? -INT16_MAX : (int16_t)v;
    }
    free(stereo);
    return out;
}

int ZistarAudioCodec::Write(const int16_t* data, int samples) {
    if (!output_enabled_) return 0;
    /* Convert 16-bit samples to 32-bit with volume (matches NoAudioCodec) */
    int32_t* buf = (int32_t*)malloc(samples * sizeof(int32_t));
    if (!buf) return 0;
    int32_t vol_factor = (int32_t)((output_volume_ / 100.0f) * 65536.0f);
    for (int i = 0; i < samples; i++) {
        int64_t v = (int64_t)data[i] * vol_factor;
        buf[i] = (v > INT32_MAX) ? INT32_MAX : (v < INT32_MIN) ? INT32_MIN : (int32_t)v;
    }
    size_t written = 0;
    i2s_channel_write(tx_handle_, buf, samples * sizeof(int32_t), &written, portMAX_DELAY);
    free(buf);
    return written / sizeof(int32_t);
}
