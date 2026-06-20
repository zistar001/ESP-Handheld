#pragma once
#include "esp_err.h"
#include <stdbool.h>

void wifi_audio_load_settings(void);
void wifi_audio_set_pc_last_octet(int octet);
void wifi_audio_rebuild_pc_ip(void);
int  wifi_audio_get_pc_last_octet(void);
const char *wifi_audio_get_pc_ip(void);
esp_err_t wifi_audio_voice_start(void);
void wifi_audio_voice_stop(void);
bool wifi_audio_is_streaming(void);
void wifi_audio_ptt_send(bool start);
