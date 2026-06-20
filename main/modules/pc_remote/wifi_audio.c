#include "wifi_audio.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "box_audio_codec.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define NVS_NS "voice"

static const char *TAG = "WIFI_AUDIO";
static bool streaming = false;
static int sock = -1;
static struct sockaddr_in dest_addr;
static TaskHandle_t audio_task_h = NULL;
char s_pc_ip[16] = "";
static int s_pc_last_octet = 0;  /* set via IP input UI */

void wifi_audio_set_pc_ip(const char *ip) {
    if (ip && strlen(ip) >= 7) {
        strncpy(s_pc_ip, ip, sizeof(s_pc_ip)-1);
        /* Also parse last octet for later rebuild */
        const char *dot = strrchr(ip, '.');
        if (dot) s_pc_last_octet = atoi(dot + 1);
        ESP_LOGI(TAG, "PC IP set: %s (octet=%d)", s_pc_ip, s_pc_last_octet);
    }
}

void wifi_audio_set_pc_last_octet(int octet) {
    s_pc_last_octet = octet;
    /* Save to NVS */
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, "pc_octet", octet);
        nvs_commit(h);
        nvs_close(h);
    }
    /* Rebuild full IP from ESP's current subnet */
    const char *my_ip = wifi_manager_get_ip();
    if (my_ip && my_ip[0]) {
        char ip[16]; strncpy(ip, my_ip, 15); ip[15]='\0';
        char *dot = strrchr(ip, '.');
        if (dot) { snprintf(dot+1, 15-(dot+1-ip), "%d", octet); strncpy(s_pc_ip, ip, sizeof(s_pc_ip)-1); }
    }
    ESP_LOGI(TAG, "PC last octet set: %d → ip=%s", octet, s_pc_ip);
}

/* Load saved octet from NVS on boot */
void wifi_audio_load_settings(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        int32_t val = 0;
        if (nvs_get_i32(h, "pc_octet", &val) == ESP_OK && val > 0) {
            s_pc_last_octet = (int)val;
            ESP_LOGI(TAG, "Loaded PC octet from NVS: %d", s_pc_last_octet);
        }
        nvs_close(h);
    }
    if (s_pc_last_octet > 0) wifi_audio_rebuild_pc_ip();
}

const char *wifi_audio_get_pc_ip(void) { return s_pc_ip; }
int wifi_audio_get_pc_last_octet(void) { return s_pc_last_octet; }

/* Rebuild IP from subnet when WiFi connects */
void wifi_audio_rebuild_pc_ip(void) {
    if (s_pc_last_octet > 0) {
        const char *my_ip = wifi_manager_get_ip();
        if (my_ip && my_ip[0]) {
            char ip[16]; strncpy(ip, my_ip, 15); ip[15]='\0';
            char *dot = strrchr(ip, '.');
            if (dot) { snprintf(dot+1, 15-(dot+1-ip), "%d", s_pc_last_octet); strncpy(s_pc_ip, ip, sizeof(s_pc_ip)-1); }
            ESP_LOGI(TAG, "PC IP rebuilt: %s", s_pc_ip);
        }
    }
}

static void audio_tx_task(void *arg) {
    int16_t buf[256];
    int pkt_count = 0;
    while (streaming) {
        esp_err_t r = box_audio_read(buf, 256);
        if (r == ESP_OK) {
            int sent = sendto(sock, buf, sizeof(buf), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
            if (++pkt_count == 1)
                ESP_LOGI(TAG, "First pkt sent to %s, ret=%d", s_pc_ip, sent);
        }
        vTaskDelay(pdMS_TO_TICKS(16));  /* 256 samples / 16ms = 16kHz matched */
    }
    if (sock >= 0) close(sock);
    sock = -1;
    vTaskDelete(NULL);
}

esp_err_t wifi_audio_voice_start(void) {
    if (streaming) return ESP_OK;
    if (!wifi_manager_is_connected()) { ESP_LOGW(TAG, "Voice fail: WiFi not connected"); return ESP_FAIL; }
    if (s_pc_ip[0] == '\0') { ESP_LOGW(TAG, "Voice fail: PC IP empty (octet=%d)", s_pc_last_octet); return ESP_FAIL; }

    /* Max mic gain during voice for best SNR */
    extern void box_audio_set_mic_gain(uint8_t db);
    box_audio_set_mic_gain(37);
    ESP_LOGI(TAG, "Voice → %s", s_pc_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9210);
    dest_addr.sin_addr.s_addr = inet_addr(s_pc_ip);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { ESP_LOGE(TAG, "socket fail"); return ESP_FAIL; }
    int bc = 1; setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

    streaming = true;
    xTaskCreatePinnedToCore(audio_tx_task, "audio_tx", 4096, NULL, 3, &audio_task_h, 1);
    return ESP_OK;
}

void wifi_audio_voice_stop(void) {
    if (!streaming) return;
    streaming = false;
    if (audio_task_h) { vTaskDelay(pdMS_TO_TICKS(100)); audio_task_h = NULL; }
    ESP_LOGI(TAG, "Voice stopped");
}

bool wifi_audio_is_streaming(void) { return streaming; }

void wifi_audio_ptt_send(bool start) {
    if (s_pc_ip[0] == '\0') return;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return;
    struct sockaddr_in a;
    a.sin_family = AF_INET;
    a.sin_port = htons(9212);
    a.sin_addr.s_addr = inet_addr(s_pc_ip);
    const char *msg = start ? "start" : "stop";
    sendto(s, msg, strlen(msg), 0, (struct sockaddr*)&a, sizeof(a));
    close(s);
}

