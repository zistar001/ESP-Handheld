#include "battery_monitor.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"

static const char *TAG = "BATTERY";
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_calibrated = false;

static const int BATTERY_MIN_MV = 3000; /* 3.0V — empty */
static const int BATTERY_MAX_MV = 4200; /* 4.2V — full */
static const int VOLTAGE_DIVIDER = 3;   /* 1:2 divider → multiply raw by 3 */

esp_err_t battery_monitor_init(void)
{
    /* Configure CHRG and DONE as input with pull-up */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BSP_BAT_CHRG) | (1ULL << BSP_BAT_DONE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* Initialize ADC1 for battery voltage reading */
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BSP_BAT_ADC_CH, &chan_cfg));

    /* Try curve-fitting calibration first */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BSP_BAT_ADC_CH,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_calibrated = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle) == ESP_OK);
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!s_calibrated) {
        adc_cali_line_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        s_calibrated = (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle) == ESP_OK);
    }
#endif

    if (!s_calibrated) {
        ESP_LOGW(TAG, "ADC calibration not available — using raw values");
    }

    ESP_LOGI(TAG, "Battery monitor initialized (calibrated=%d)", s_calibrated);
    return ESP_OK;
}

int battery_monitor_get_millivolts(void)
{
    if (!s_adc_handle) return -1;

    int adc_raw = 0;
    /* Average 10 samples for stability */
    int64_t sum = 0;
    for (int i = 0; i < 10; i++) {
        if (adc_oneshot_read(s_adc_handle, BSP_BAT_ADC_CH, &adc_raw) == ESP_OK) {
            sum += adc_raw;
        }
        esp_rom_delay_us(100);
    }
    adc_raw = (int)(sum / 10);

    int mv = 0;
    if (s_calibrated) {
        adc_cali_raw_to_voltage(s_cali_handle, adc_raw, &mv);
    } else {
        /* Uncalibrated: approximate 12-bit ADC at 3.3V ref */
        mv = (adc_raw * 3300) / 4095;
    }

    /* Apply voltage divider correction */
    return mv * VOLTAGE_DIVIDER;
}

int battery_monitor_get_percentage(void)
{
    int mv = battery_monitor_get_millivolts();
    if (mv < 0) return -1;

    if (mv >= BATTERY_MAX_MV) return 100;
    if (mv <= BATTERY_MIN_MV) return 0;

    return (mv - BATTERY_MIN_MV) * 100 / (BATTERY_MAX_MV - BATTERY_MIN_MV);
}

bool battery_monitor_is_charging(void)
{
    /* CHRG low + DONE high = charging */
    return (gpio_get_level(BSP_BAT_CHRG) == 0 && gpio_get_level(BSP_BAT_DONE) == 1);
}

bool battery_monitor_is_charged(void)
{
    /* Both CHRG and DONE high = charge complete (or no battery) */
    return (gpio_get_level(BSP_BAT_CHRG) == 1 && gpio_get_level(BSP_BAT_DONE) == 1);
}
