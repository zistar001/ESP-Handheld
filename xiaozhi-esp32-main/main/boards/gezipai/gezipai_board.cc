#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#define TAG "gezipai"
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_2
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#define BATTERY_READ_INTERVAL_MS (10 * 1000) // 10 秒

#define AUDIO_INPUT_REFERENCE true

/* Combo key pins: KEY_START (GPIO15) + KEY_B (GPIO16) held together returns to ESP_BSP */
#define COMBO_PIN_START    GPIO_NUM_15
#define COMBO_PIN_B        GPIO_NUM_16
#define COMBO_DEBOUNCE_MS  150  /* ~3 samples at 50ms */

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class GezipaiBoard : public WifiBoard
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay *display_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    bool adc_calibrated_;
    esp_timer_handle_t battery_timer_;
    esp_timer_handle_t combo_timer_;
    int last_battery_voltage_;
    int last_battery_percentage_;
    bool led_on_ = false;

    // 电池定时器回调函数
    static void BatteryTimerCallback(void *arg)
    {
        static uint8_t timer_count_;
        timer_count_++;
        if (timer_count_ >= 10)
        {
            GezipaiBoard *board = static_cast<GezipaiBoard *>(arg);
            board->UpdateBatteryStatus();
            timer_count_ = 0;
        }

#ifdef BUILTIN_LED_GPIO
        gpio_set_level(BUILTIN_LED_GPIO, timer_count_ % 2);
#endif
    }

    // 更新电池状态
    void UpdateBatteryStatus()
    {
        last_battery_voltage_ = ReadBatteryVoltage();
        last_battery_percentage_ = CalculateBatteryPercentage(last_battery_voltage_);

        if (adc_calibrated_)
        {
            // ESP_LOGI(TAG, "电池状态更新 - 电压: %d mV, 电量: %d%%",
            //          last_battery_voltage_, last_battery_percentage_);
        }
        else
        {
            ESP_LOGI(TAG, "电池ADC未校准，无法准确读取电压");
        }
    }

    // 返回手持设备主界面 (ESP_BSP)
    static void ReturnToHandheld()
    {
        ESP_LOGI(TAG, "Returning to handheld main menu...");
        const esp_partition_t *factory = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
        if (factory)
        {
            esp_ota_set_boot_partition(factory);
            esp_restart();
        }
        else
        {
            ESP_LOGE(TAG, "Factory partition not found!");
        }
    }

    void InitializeCodecI2c()
    {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            } });
        boot_button_.OnPressDown([this]()
                                 { Application::GetInstance().StartListening(); });
        boot_button_.OnPressUp([this]()
                               { Application::GetInstance().StopListening(); });
        volume_up_button_.OnClick([this]()
                                  {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); });

        volume_up_button_.OnLongPress([this]()
                                      {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME); });

        volume_down_button_.OnClick([this]()
                                    {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); });
    }

    void InitializeSt7789Display()
    {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 60 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        ESP_LOGI(TAG, "Install ST7789 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_LOGI(TAG, "ST7789V init complete (original ESP-IDF driver)");

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTools()
    {
    }

    void InitializeBatteryAdc()
    {
        // 初始化 ADC1 单次采样模式
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle_));

        // 配置 ADC 通道
        adc_oneshot_chan_cfg_t config = {
            .atten = BATTERY_ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, BATTERY_ADC_CHANNEL, &config));

        // 初始化校准
        adc_cali_handle_t handle = NULL;
        esp_err_t ret = ESP_FAIL;
        adc_calibrated_ = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        if (!adc_calibrated_)
        {
            ESP_LOGI(TAG, "使用曲线拟合校准方案");
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .chan = BATTERY_ADC_CHANNEL,
                .atten = BATTERY_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
            if (ret == ESP_OK)
            {
                adc_calibrated_ = true;
            }
        }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        if (!adc_calibrated_)
        {
            ESP_LOGI(TAG, "使用线性拟合校准方案");
            adc_cali_line_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = BATTERY_ADC_ATTEN,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
            if (ret == ESP_OK)
            {
                adc_calibrated_ = true;
            }
        }
#endif

        adc1_cali_handle_ = handle;
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "ADC校准成功");
        }
        else if (ret == ESP_ERR_NOT_SUPPORTED || !adc_calibrated_)
        {
            ESP_LOGW(TAG, "eFuse未烧录，跳过软件校准");
        }
        else
        {
            ESP_LOGE(TAG, "无效参数或内存不足");
        }
    }

    void InitializeTimer()
    {
        esp_timer_create_args_t timer_args = {
            .callback = &BatteryTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_timer",
            .skip_unhandled_events = false,
        };

        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &battery_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(battery_timer_, 1000 * 1000)); // 微秒为单位
    }

    // START+B 组合键定时器回调
    static void ComboTimerCallback(void *arg)
    {
        GezipaiBoard *board = static_cast<GezipaiBoard *>(arg);
        board->CheckComboKey();
    }

    // 检测 START+B 组合键（同时按下约150ms触发返回手持机）
    void CheckComboKey()
    {
        static int combo_count = 0;
        if (gpio_get_level(COMBO_PIN_START) == 0 && gpio_get_level(COMBO_PIN_B) == 0) {
            combo_count++;
            if (combo_count >= (COMBO_DEBOUNCE_MS / 50)) {
                combo_count = 0;
                ESP_LOGI(TAG, "START+B combo detected, returning to handheld");
                ReturnToHandheld();  // calls esp_restart() — no return
            }
        } else {
            combo_count = 0;
        }
    }

    // 读取电池电压(mV)
    int ReadBatteryVoltage()
    {
        int adc_raw = 0;
        long long sum = 0;
        int voltage = 0;
        for (int i = 0; i < 10; i++)
        {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle_, BATTERY_ADC_CHANNEL, &adc_raw));
            sum += adc_raw;
        }
        adc_raw = sum / 10;

        // ESP_LOGI(TAG, "电池ADC原始数据: %d", adc_raw);

        if (adc_calibrated_)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle_, adc_raw, &voltage));
        }

        // 分压电阻1:2，所以乘以2
        return voltage * 3;
    }

    // 计算电池电量百分比
    int CalculateBatteryPercentage(int voltage)
    {
        if (!adc_calibrated_ || voltage <= 0)
        {
            return -1; // 返回-1表示无法读取
        }

        // 假设电池电压范围为3.0V-4.2V
        const int min_voltage = 3000; // 3.0V
        const int max_voltage = 4200; // 4.2V

        if (voltage < min_voltage)
        {
            return 0;
        }
        else if (voltage > max_voltage)
        {
            return 100;
        }
        else
        {
            return (voltage - min_voltage) * 100 / (max_voltage - min_voltage);
        }
    }

public:
    GezipaiBoard() : boot_button_(BOOT_BUTTON_GPIO),
                     volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                     volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
                     last_battery_voltage_(0), last_battery_percentage_(-1)
    {
        ESP_LOGI(TAG, "Initializing Gezipai Board");
#ifdef BUILTIN_LED_GPIO
        // init gpio led
        gpio_config_t led_config = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&led_config));
#endif

        InitializeCodecI2c();
        InitializeSpi();
        InitializeButtons();
        InitializeSt7789Display();
        InitializeTools();
        InitializeBatteryAdc();

        // 初始读取一次电量
        UpdateBatteryStatus();

        // 启动定时器定
        InitializeTimer();

        // 初始化组合键 GPIO（START+B 返回手持机）
        gpio_config_t combo_io = {
            .pin_bit_mask = (1ULL << COMBO_PIN_START) | (1ULL << COMBO_PIN_B),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&combo_io);

        // 组合键检测定时器（50ms间隔）
        esp_timer_create_args_t combo_timer_args = {
            .callback = &ComboTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "combo_timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&combo_timer_args, &combo_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(combo_timer_, 50 * 1000)); // 50ms

        GetBacklight()->RestoreBrightness();
    }

    ~GezipaiBoard()
    {
        // 停止定时器
        esp_timer_stop(combo_timer_);
        esp_timer_delete(combo_timer_);
        esp_timer_stop(battery_timer_);
        esp_timer_delete(battery_timer_);

        // 释放ADC资源
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle_));

        if (adc_calibrated_)
        {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle_));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(adc1_cali_handle_));
#endif
        }
    }

    virtual Led *GetLed() override
    {
        static NoLed led;
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        // static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        //                                     AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
        //                                     AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, true);
        static BoxAudioCodec audio_codec(
            codec_i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    // 获取最后一次读取的电池电压(mV)
    int GetBatteryVoltage()
    {
        return last_battery_voltage_;
    }

    // 获取最后一次计算的电池电量百分比
    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging)
    {
        level = last_battery_percentage_;

        uint8_t chargeLevel = gpio_get_level(CHARGING_GPIO);
        uint8_t doneLevel = gpio_get_level(DONE_GPIO);
        if (chargeLevel == 0 && doneLevel == 0)
        {
            charging = false;
            discharging = true;
        }
        else if (chargeLevel == 0 && doneLevel == 1)
        {
            charging = true;
            discharging = false;
        }
        else if (chargeLevel == 1 && doneLevel == 0)
        {
            charging = false;
            discharging = true;
        }

        return true;
    }
};

DECLARE_BOARD(GezipaiBoard);
