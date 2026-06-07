#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t time_sync_init(void);
bool time_sync_is_synced(void);
const char *time_sync_get_time_string(void);
const char *time_sync_get_date_string(void);
