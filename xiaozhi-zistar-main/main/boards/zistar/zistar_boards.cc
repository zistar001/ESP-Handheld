#include "wifi_board.h"
#include "zistar_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>

#define TAG "ZistarBoard"

/* Combo key pins: KEY_START (GPIO6) + KEY_B (GPIO5) held together returns to ESP_BSP */
#define COMBO_PIN_START    GPIO_NUM_6
#define COMBO_PIN_B        GPIO_NUM_5
#define COMBO_DEBOUNCE_MS  150

/* 电池 — 无ADC读取（IO3被AMP占用），无充电检测GPIO */
/* 始终报告有电但不显示具体百分比 */
#define HAS_BATTERY false

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class ZistarBoard : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay* display_;
    esp_timer_handle_t combo_timer_;

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

    // SPI初始化（显示屏用 SPI2_HOST）
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 按钮初始化
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnPressDown([this]()
                                 { Application::GetInstance().StartListening(); });
        boot_button_.OnPressUp([this]()
                               { Application::GetInstance().StopListening(); });
        volume_up_button_.OnClick([this]()
                                  {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) volume = 100;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); });
        volume_up_button_.OnLongPress([this]()
                                      { GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME); });
        volume_down_button_.OnClick([this]()
                                    {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) volume = 0;
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume)); });
    }

    // 显示屏初始化 ST7789
    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 60 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // MCP Tools 初始化
    void InitializeTools() {
    }

    // START+B 组合键定时器回调
    static void ComboTimerCallback(void *arg) {
        ZistarBoard *board = static_cast<ZistarBoard *>(arg);
        board->CheckComboKey();
    }

    void CheckComboKey() {
        static int combo_count = 0;
        if (gpio_get_level(COMBO_PIN_START) == 0 && gpio_get_level(COMBO_PIN_B) == 0) {
            combo_count++;
            if (combo_count >= (COMBO_DEBOUNCE_MS / 50)) {
                combo_count = 0;
                ESP_LOGI(TAG, "START+B combo detected, returning to handheld");
                ReturnToHandheld();
            }
        } else {
            combo_count = 0;
        }
    }

    // LED 初始化
    void InitializeLed() {
#ifdef BUILTIN_LED_GPIO
        gpio_config_t led_config = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&led_config));
#endif
    }

public:
    ZistarBoard() : boot_button_(BOOT_BUTTON_GPIO),
                    volume_up_button_(VOLUME_UP_BUTTON_GPIO),
                    volume_down_button_(VOLUME_DOWN_BUTTON_GPIO)
    {
        ESP_LOGI(TAG, "Initializing Zistar Board (Rev 2)");

        InitializeLed();
        InitializeSpi();
        InitializeButtons();
        InitializeDisplay();
        InitializeTools();

        // 组合键检测定时器（50ms间隔）
        esp_timer_create_args_t combo_timer_args = {
            .callback = &ComboTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "combo_timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&combo_timer_args, &combo_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(combo_timer_, 50 * 1000));

        GetBacklight()->RestoreBrightness();
    }

    ~ZistarBoard() {
        esp_timer_stop(combo_timer_);
        esp_timer_delete(combo_timer_);
    }

    virtual Led *GetLed() override {
        static NoLed led;
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override {
        static ZistarAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            GPIO_NUM_NC);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual Backlight *GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override {
        // 无电池ADC，始终报告100%（不显示低电量警告）
        level = 100;
        charging = false;
        discharging = false;
        return true;
    }
};

DECLARE_BOARD(ZistarBoard);
