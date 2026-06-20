#include "ble_hid.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_hidd.h"
#include "esp_hidd_gatts.h"

static const char *TAG = "BLE_HID";
static esp_hidd_dev_t *hid_dev = NULL;
static bool connected = false;
static bool s_initialized = false;

// Keyboard report (report ID 1)
static const uint8_t keybd_report_map[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x05,
    0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
    0xC0
};

// Mouse report (report ID 2)
static const uint8_t mouse_report_map[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02,
    0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
    0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08,
    0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    { .data = keybd_report_map, .len = sizeof(keybd_report_map) },
    { .data = mouse_report_map, .len = sizeof(mouse_report_map) }
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id         = 0x16C0,
    .product_id        = 0x05DF,
    .version           = 0x0100,
    .device_name       = "ESP-Handheld",
    .manufacturer_name = "Espressif",
    .serial_number     = "1234567890",
    .report_maps       = ble_report_maps,
    .report_maps_len   = 2
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (!param->ble_security.auth_cmpl.success)
            ESP_LOGE(TAG, "AUTH ERROR: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        break;
    default:
        break;
    }
}

static void hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID start, advertising...");
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min = 0x20, .adv_int_max = 0x30,
            .adv_type = ADV_TYPE_IND, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL, .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
        });
        break;
    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID connected");
        connected = true;
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID disconnected");
        connected = false;
        esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
            .adv_int_min = 0x20, .adv_int_max = 0x30,
            .adv_type = ADV_TYPE_IND, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL, .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY
        });
        break;
    default:
        break;
    }
}

esp_err_t ble_hid_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "BLE HID already initialized, skipping");
        return ESP_OK;
    }
    s_initialized = true;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));

    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;   /* SC + bond, no MITM */
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;                  /* No display — no passkey prompt */
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t key_size = 16;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, 1);
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, 1);

    esp_ble_gap_set_device_name(ble_hid_config.device_name);

    const uint8_t hidd_uuid[] = { 0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x12,0x18,0x00,0x00 };
    esp_ble_gap_config_adv_data(&(esp_ble_adv_data_t){
        .set_scan_rsp = false, .include_name = true, .include_txpower = true,
        .min_interval = 0x0006, .max_interval = 0x0010,
        .appearance = ESP_HID_APPEARANCE_KEYBOARD,
        .manufacturer_len = 0, .p_manufacturer_data = NULL,
        .service_data_len = 0, .p_service_data = NULL,
        .service_uuid_len = sizeof(hidd_uuid),
        .p_service_uuid = (uint8_t *)hidd_uuid,
        .flag = 0x6
    });

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler));
    ESP_ERROR_CHECK(esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, hidd_event_callback, &hid_dev));

    ESP_LOGI(TAG, "BLE HID ready");
    return ESP_OK;
}

esp_err_t ble_hid_send_key(uint8_t modifier, uint8_t key) {
    if (!connected || !hid_dev) return ESP_ERR_INVALID_STATE;
    uint8_t report[8] = { modifier, 0, key, 0, 0, 0, 0, 0 };
    esp_err_t r = esp_hidd_dev_input_set(hid_dev, 0, 1, report, 8);
    vTaskDelay(pdMS_TO_TICKS(10));
    memset(report, 0, 8);
    esp_hidd_dev_input_set(hid_dev, 0, 1, report, 8);
    if (r != ESP_OK) ESP_LOGW(TAG, "send_key fail: %s", esp_err_to_name(r));
    return r;
}

void ble_hid_press_key(uint8_t modifier, uint8_t key) {
    if (!connected || !hid_dev) return;
    uint8_t report[8] = { modifier, 0, key, 0, 0, 0, 0, 0 };
    esp_hidd_dev_input_set(hid_dev, 0, 1, report, 8);
}

void ble_hid_send_combo(uint8_t modifier, uint8_t key) {
    if (!connected || !hid_dev) return;
    /* Step 1: press modifier alone (50ms) — lets host register the modifier state */
    uint8_t r[8] = { modifier, 0, 0, 0, 0, 0, 0, 0 };
    esp_hidd_dev_input_set(hid_dev, 0, 1, r, 8);
    vTaskDelay(pdMS_TO_TICKS(50));
    /* Step 2: add the key (50ms) — combo held */
    r[2] = key;
    esp_hidd_dev_input_set(hid_dev, 0, 1, r, 8);
    vTaskDelay(pdMS_TO_TICKS(50));
    /* Step 3: release the key (20ms) — modifier still held */
    r[2] = 0;
    esp_hidd_dev_input_set(hid_dev, 0, 1, r, 8);
    vTaskDelay(pdMS_TO_TICKS(20));
    /* Step 4: release modifier */
    memset(r, 0, 8);
    esp_hidd_dev_input_set(hid_dev, 0, 1, r, 8);
}

esp_err_t ble_hid_send_modkey(uint8_t modifier, uint8_t key) {
    if (!connected || !hid_dev) return ESP_ERR_INVALID_STATE;
    /* Send modifier alone first, then add key — some BLE stacks need this */
    uint8_t report[8] = { modifier, 0, 0, 0, 0, 0, 0, 0 };
    esp_hidd_dev_input_set(hid_dev, 0, 1, report, 8);
    vTaskDelay(pdMS_TO_TICKS(5));
    report[2] = key;
    esp_hidd_dev_input_set(hid_dev, 0, 1, report, 8);
    vTaskDelay(pdMS_TO_TICKS(10));
    memset(report, 0, 8);
    esp_hidd_dev_input_set(hid_dev, 0, 1, report, 8);
    return ESP_OK;
}

esp_err_t ble_hid_send_mouse(uint8_t buttons, int8_t dx, int8_t dy) {
    if (!connected || !hid_dev) return ESP_ERR_INVALID_STATE;
    uint8_t report[4] = { buttons, (uint8_t)dx, (uint8_t)dy, 0 };
    esp_err_t r = esp_hidd_dev_input_set(hid_dev, 0, 2, report, 4);
    if (r != ESP_OK) ESP_LOGW(TAG, "send_mouse fail: %s", esp_err_to_name(r));
    return r;
}


void ble_hid_mouse_click(uint8_t buttons) {
    if (!hid_dev) return;
    uint8_t report[4] = { buttons, 0, 0, 0 };
    esp_hidd_dev_input_set(hid_dev, 0, 2, report, 4);
    vTaskDelay(pdMS_TO_TICKS(10));
    memset(report, 0, 4);
    esp_hidd_dev_input_set(hid_dev, 0, 2, report, 4);
}

void ble_hid_release_all(void) {
    if (!hid_dev) return;
    uint8_t zero[8] = {0};
    esp_hidd_dev_input_set(hid_dev, 0, 1, zero, 8);
}

esp_err_t ble_hid_deinit(void) {
    if (hid_dev) {
        esp_hidd_dev_deinit(hid_dev);
        hid_dev = NULL;
    }
    connected = false;
    s_initialized = false;
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    ESP_LOGI(TAG, "BLE HID deinitialized, BT memory released");
    return ESP_OK;
}

bool ble_hid_is_connected(void) { return connected; }
const char *ble_hid_get_passkey(void) { return "1234"; }
