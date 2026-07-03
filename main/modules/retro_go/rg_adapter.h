#pragma once
#include <stdbool.h>
#include "bsp/key_driver.h"

void rg_wrapper_init(void);
bool rg_wrapper_check_exit(void);
void rg_start(const char *rom_path);
void rg_stop(void);
bool rg_is_running(void);
void rg_key_event(key_id_t key, bool pressed);
