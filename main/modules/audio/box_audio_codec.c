#include "box_audio_codec.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

static const char *TAG = "BOX_AUDIO";

#define AUDIO_BITS        16

static uint32_t s_sample_rate = 16000;

/* Shared I2S handles (TX + RX on same port) */
static i2s_chan_handle_t            s_tx_chan  = NULL;
static i2s_chan_handle_t            s_rx_chan  = NULL;
static const audio_codec_data_if_t *s_data_if  = NULL;

/* ES8311 (output) */
static const audio_codec_ctrl_if_t *s_out_ctrl_if  = NULL;
static const audio_codec_gpio_if_t *s_gpio_if      = NULL;
static const audio_codec_if_t      *s_out_codec_if = NULL;
static esp_codec_dev_handle_t       s_out_dev      = NULL;

/* ES7210 (input) */
static const audio_codec_ctrl_if_t *s_in_ctrl_if  = NULL;
static const audio_codec_if_t      *s_in_codec_if = NULL;
static esp_codec_dev_handle_t       s_in_dev      = NULL;

/* Shared I2C bus for audio codecs */
static i2c_master_bus_handle_t s_i2c_bus = NULL;

static uint8_t s_volume = 70;
static bool s_inited = false;

static esp_err_t create_shared_i2s(uint32_t rate)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_chan, &s_rx_chan),
                        TAG, "i2s_new_channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(AUDIO_BITS, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BSP_AUDIO_MCLK,
            .bclk = BSP_AUDIO_BCLK,
            .ws   = BSP_AUDIO_WS,
            .dout = BSP_AUDIO_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {false, false, false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_chan, &std_cfg),
                        TAG, "i2s_channel_init_std_mode failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_chan),
                        TAG, "i2s_channel_enable(TX) failed");

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1
                       | I2S_TDM_SLOT2 | I2S_TDM_SLOT3,
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM,
        },
        .gpio_cfg = {
            .mclk = BSP_AUDIO_MCLK,
            .bclk = BSP_AUDIO_BCLK,
            .ws   = BSP_AUDIO_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = BSP_AUDIO_DIN,
            .invert_flags = {false, false, false},
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_tdm_mode(s_rx_chan, &tdm_cfg),
                        TAG, "i2s_channel_init_tdm_mode failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_rx_chan),
                        TAG, "i2s_channel_enable(RX) failed");

    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = s_rx_chan,
        .tx_handle = s_tx_chan,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    ESP_RETURN_ON_FALSE(s_data_if, ESP_FAIL, TAG, "audio_codec_new_i2s_data failed");

    ESP_LOGI(TAG, "I2S_NUM_0 duplex: TX(std)+RX(tdm4) @ %lu Hz", rate);
    return ESP_OK;
}

static esp_err_t create_i2c_bus(void)
{
    if (s_i2c_bus) return ESP_OK;
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = BSP_CODEC_SDA,
        .scl_io_num = BSP_CODEC_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg, &s_i2c_bus),
                        TAG, "i2c_new_master_bus failed");
    return ESP_OK;
}

/* Internal: open codecs after I2S is up */
static esp_err_t open_codecs(uint32_t rate)
{
    /* ES8311 DAC */
    {
        audio_codec_i2c_cfg_t i2c_cfg = {
            .addr = ES8311_CODEC_DEFAULT_ADDR,
            .bus_handle = s_i2c_bus,
        };
        s_out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        ESP_RETURN_ON_FALSE(s_out_ctrl_if, ESP_FAIL, TAG, "ES8311 I2C ctrl failed");

        s_gpio_if = audio_codec_new_gpio();
        ESP_RETURN_ON_FALSE(s_gpio_if, ESP_FAIL, TAG, "gpio_if failed");

        es8311_codec_cfg_t es8311_cfg = {
            .ctrl_if = s_out_ctrl_if,
            .gpio_if = s_gpio_if,
            .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
            .pa_pin = BSP_AUDIO_PA_PIN,
            .use_mclk = true,
            .hw_gain = { .pa_voltage = 5.0, .codec_dac_voltage = 3.3 },
            .pa_reverted = false,
        };
        s_out_codec_if = es8311_codec_new(&es8311_cfg);
        ESP_RETURN_ON_FALSE(s_out_codec_if, ESP_FAIL, TAG, "es8311_codec_new failed");

        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
            .codec_if = s_out_codec_if,
            .data_if = s_data_if,
        };
        s_out_dev = esp_codec_dev_new(&dev_cfg);
        ESP_RETURN_ON_FALSE(s_out_dev, ESP_FAIL, TAG, "esp_codec_dev_new(OUT) failed");

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = AUDIO_BITS,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = rate,
            .mclk_multiple = 0,
        };
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_out_dev, &fs),
                            TAG, "esp_codec_dev_open(OUT) failed");
        ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(s_out_dev, s_volume),
                            TAG, "set_out_vol failed");
    }

    /* ES7210 ADC */
    {
        audio_codec_i2c_cfg_t i2c_cfg = {
            .addr = ES7210_CODEC_DEFAULT_ADDR,
            .bus_handle = s_i2c_bus,
        };
        s_in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        ESP_RETURN_ON_FALSE(s_in_ctrl_if, ESP_FAIL, TAG, "ES7210 I2C ctrl failed");

        es7210_codec_cfg_t es7210_cfg = {
            .ctrl_if = s_in_ctrl_if,
            .master_mode = false,
            .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2,
        };
        s_in_codec_if = es7210_codec_new(&es7210_cfg);
        ESP_RETURN_ON_FALSE(s_in_codec_if, ESP_FAIL, TAG, "es7210_codec_new failed");

        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
            .codec_if = s_in_codec_if,
            .data_if = s_data_if,
        };
        s_in_dev = esp_codec_dev_new(&dev_cfg);
        ESP_RETURN_ON_FALSE(s_in_dev, ESP_FAIL, TAG, "esp_codec_dev_new(IN) failed");

        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = AUDIO_BITS,
            .channel = 4,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),  /* mic1 only, like XiaoZhi */
            .sample_rate = rate,
            .mclk_multiple = 0,
        };
        ESP_RETURN_ON_ERROR(esp_codec_dev_open(s_in_dev, &fs),
                            TAG, "esp_codec_dev_open(IN) failed");
    }

    return ESP_OK;
}

esp_err_t box_audio_init(void)
{
    return box_audio_init_rate(16000);
}

esp_err_t box_audio_init_rate(uint32_t sample_rate)
{
    if (s_inited && s_sample_rate == sample_rate) return ESP_OK;

    /* If already initialized at a different rate, tear down I2S + codecs only (keep I2C) */
    if (s_inited) {
        ESP_LOGI(TAG, "Reconfiguring %lu -> %lu Hz", s_sample_rate, sample_rate);
        if (s_out_dev) { esp_codec_dev_close(s_out_dev); esp_codec_dev_delete(s_out_dev); s_out_dev = NULL; }
        if (s_out_codec_if) { audio_codec_delete_codec_if(s_out_codec_if); s_out_codec_if = NULL; }
        if (s_out_ctrl_if) { audio_codec_delete_ctrl_if(s_out_ctrl_if); s_out_ctrl_if = NULL; }
        if (s_gpio_if) { audio_codec_delete_gpio_if(s_gpio_if); s_gpio_if = NULL; }
        if (s_in_dev) { esp_codec_dev_close(s_in_dev); esp_codec_dev_delete(s_in_dev); s_in_dev = NULL; }
        if (s_in_codec_if) { audio_codec_delete_codec_if(s_in_codec_if); s_in_codec_if = NULL; }
        if (s_in_ctrl_if) { audio_codec_delete_ctrl_if(s_in_ctrl_if); s_in_ctrl_if = NULL; }
        if (s_data_if) { audio_codec_delete_data_if(s_data_if); s_data_if = NULL; }
        if (s_rx_chan) { i2s_channel_disable(s_rx_chan); i2s_del_channel(s_rx_chan); s_rx_chan = NULL; }
        if (s_tx_chan) { i2s_channel_disable(s_tx_chan); i2s_del_channel(s_tx_chan); s_tx_chan = NULL; }
        s_inited = false;

        ESP_RETURN_ON_ERROR(create_shared_i2s(sample_rate), TAG, "I2S reconfig failed");
        ESP_RETURN_ON_ERROR(open_codecs(sample_rate), TAG, "Codec reopen failed");
        s_sample_rate = sample_rate;
        s_inited = true;
        ESP_LOGI(TAG, "Audio reconfigured to %lu Hz", sample_rate);
        return ESP_OK;
    }

    /* First-time init */
    s_sample_rate = sample_rate;
    ESP_RETURN_ON_ERROR(create_i2c_bus(), TAG, "I2C bus failed");
    ESP_RETURN_ON_ERROR(create_shared_i2s(sample_rate), TAG, "I2S init failed");
    ESP_RETURN_ON_ERROR(open_codecs(sample_rate), TAG, "Codec init failed");

    ESP_LOGI(TAG, "Audio ready: ES8311+ES7210 @ %lu Hz %d-bit", sample_rate, AUDIO_BITS);
    s_inited = true;
    return ESP_OK;
}

esp_err_t box_audio_read(int16_t *data, int samples)
{
    if (!s_inited || !s_in_dev) return ESP_ERR_INVALID_STATE;
    int ret = esp_codec_dev_read(s_in_dev, (void *)data, samples * sizeof(int16_t));
    return (ret == ESP_CODEC_DEV_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t box_audio_write(const int16_t *data, int samples)
{
    if (!s_inited || !s_out_dev) return ESP_ERR_INVALID_STATE;
    int ret = esp_codec_dev_write(s_out_dev, (void *)data, samples * sizeof(int16_t));
    return (ret == ESP_CODEC_DEV_OK) ? ESP_OK : ESP_FAIL;
}

void box_audio_set_volume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    s_volume = vol;
    if (s_out_dev) esp_codec_dev_set_out_vol(s_out_dev, s_volume);
}

uint8_t box_audio_get_volume(void)
{
    return s_volume;
}

void box_audio_set_mic_gain(uint8_t db)
{
    if (!s_in_dev) return;
    esp_codec_dev_set_in_channel_gain(s_in_dev,
        ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1), db);
}

void box_audio_stop(void)
{
    if (s_out_dev) { esp_codec_dev_close(s_out_dev); esp_codec_dev_delete(s_out_dev); s_out_dev = NULL; }
    if (s_out_codec_if) { audio_codec_delete_codec_if(s_out_codec_if); s_out_codec_if = NULL; }
    if (s_out_ctrl_if) { audio_codec_delete_ctrl_if(s_out_ctrl_if); s_out_ctrl_if = NULL; }
    if (s_gpio_if) { audio_codec_delete_gpio_if(s_gpio_if); s_gpio_if = NULL; }
    if (s_in_dev) { esp_codec_dev_close(s_in_dev); esp_codec_dev_delete(s_in_dev); s_in_dev = NULL; }
    if (s_in_codec_if) { audio_codec_delete_codec_if(s_in_codec_if); s_in_codec_if = NULL; }
    if (s_in_ctrl_if) { audio_codec_delete_ctrl_if(s_in_ctrl_if); s_in_ctrl_if = NULL; }
    if (s_data_if) { audio_codec_delete_data_if(s_data_if); s_data_if = NULL; }
    if (s_rx_chan) { i2s_channel_disable(s_rx_chan); i2s_del_channel(s_rx_chan); s_rx_chan = NULL; }
    if (s_tx_chan) { i2s_channel_disable(s_tx_chan); i2s_del_channel(s_tx_chan); s_tx_chan = NULL; }
    if (s_i2c_bus) { i2c_del_master_bus(s_i2c_bus); s_i2c_bus = NULL; }
    s_inited = false;
}

bool box_audio_is_inited(void)
{
    return s_inited;
}

uint32_t box_audio_get_sample_rate(void)
{
    return s_sample_rate;
}



/* Expose raw I2S TX handle for direct writes (bypass codec layer) */
i2s_chan_handle_t box_audio_get_tx_channel(void)
{
    return s_tx_chan;
}
