#pragma once
#include "esp_err.h"

/* 天气地区映射表条目 */
typedef struct {
    const char *id;      /* HeFeng location ID */
    const char *name;    /* 显示名称，如"安溪县" */
} weather_loc_entry_t;

esp_err_t weather_init(void);
void weather_set_api_key(const char *key);
void weather_set_location(const char *loc);  /* 设置地区 ID 并持久化到 NVS */
void weather_set_location_by_index(int index);  /* 按索引设置地区 */
int weather_get_location_index(void);  /* 获取当前地区在列表中的索引 */
const char *weather_get_location_id(void);   /* 获取当前地区 ID  */
const char *weather_get_location_name(void);  /* 获取当前地区名 */
const weather_loc_entry_t *weather_get_locations(void);  /* 所有可选地区列表 */
int weather_get_location_count(void);
void weather_request_refresh(void);  /* 触发立即刷新天气（任务通知机制） */
