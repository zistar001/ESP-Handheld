#include "wifi_audio.h"
#include "modules/wifi_manager/wifi_manager.h"
#include "box_audio_codec.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WIFI_AUDIO";
static bool streaming = false;
static bool connected = false;
static int sock = -1;
static struct sockaddr_in dest_addr;
static TaskHandle_t audio_task_h = NULL;

static void audio_tx_task(void *arg);

esp_err_t wifi_audio_connect(const char *ssid, const char *pass) {
    wifi_manager_init();
    if (!ssid || !*ssid) {
        ESP_LOGW(TAG, "No SSID configured");
        return ESP_ERR_INVALID_ARG;
    }
    wifi_config_t wc = { 0 };
    strncpy((char*)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char*)wc.sta.password, pass ? pass : "", sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_connect();
    ESP_LOGI(TAG, "Connecting to %s", ssid);
    connected = true;
    return ESP_OK;
}

void wifi_audio_disconnect(void) {
    esp_wifi_disconnect();
    connected = false;
    ESP_LOGI(TAG, "WiFi disconnected");
}

bool wifi_audio_is_connected(void) { return connected; }

esp_err_t wifi_audio_start(const char *pc_ip, uint16_t port) {
    if (!pc_ip) return ESP_ERR_INVALID_ARG;

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    struct hostent *he = gethostbyname(pc_ip);
    if (he) {
        dest_addr.sin_addr = *(struct in_addr*)he->h_addr;
    } else {
        ESP_LOGE(TAG, "DNS lookup failed: %s", pc_ip);
        return ESP_FAIL;
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed");
        return ESP_FAIL;
    }

    streaming = true;
    xTaskCreatePinnedToCore(audio_tx_task, "audio_tx", 4096, NULL, 5,
                            &audio_task_h, 1);
    ESP_LOGI(TAG, "Streaming audio to %s:%d", pc_ip, port);
    return ESP_OK;
}

static void audio_tx_task(void *arg) {
    int16_t buf[512];
    while (streaming) {
        /* Read PCM from ES7210 via shared box_audio_codec */
        esp_err_t r = box_audio_read(buf, 256);
        if (r == ESP_OK) {
            sendto(sock, buf, sizeof(buf), 0,
                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        }
        vTaskDelay(pdMS_TO_TICKS(10));  /* ~16ms @ 256 samples / 16kHz */
    }
    if (sock >= 0) close(sock);
    sock = -1;
    vTaskDelete(NULL);
}

void wifi_audio_stop(void) {
    streaming = false;
    if (audio_task_h) {
        vTaskDelay(pdMS_TO_TICKS(100));
        audio_task_h = NULL;
    }
    ESP_LOGI(TAG, "Audio streaming stopped");
}

bool wifi_audio_is_streaming(void) { return streaming; }
