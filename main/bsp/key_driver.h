#ifndef KEY_DRIVER_H
#define KEY_DRIVER_H
#include "esp_err.h"

typedef enum {
    KEY_UP=0, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_START, KEY_MAX
} key_id_t;

typedef void (*key_cb_t)(key_id_t key, bool pressed);

esp_err_t key_driver_init(key_cb_t cb);
void key_driver_scan_task(void *arg);

#endif
