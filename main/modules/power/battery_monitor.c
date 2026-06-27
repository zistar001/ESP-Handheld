#include "battery_monitor.h"
#include "esp_log.h"

/*
 * Rev 2 PCB: 无电池ADC检测（IO3被MAX98357 DIN占用），
 * 无充电状态GPIO。电池存在但无法测量电压或充电状态。
 * 所有接口返回保守默认值以避免低电量警告。
 */

static const char *TAG = "BATTERY";

esp_err_t battery_monitor_init(void)
{
    ESP_LOGI(TAG, "Battery monitor: no ADC (Rev 2 PCB), always reporting 100%%");
    return ESP_OK;
}

int battery_monitor_get_millivolts(void)
{
    return -1; /* 无法测量 */
}

int battery_monitor_get_percentage(void)
{
    return 100; /* 始终报告满电 */
}

bool battery_monitor_is_charging(void)
{
    return false;
}

bool battery_monitor_is_charged(void)
{
    return true;
}
