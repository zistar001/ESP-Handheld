#include "sd_card.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"

static const char *TAG = "SD";
static const char *MP = "/sdcard";
static bool mounted = false;

esp_err_t sd_card_init(void) {
    ESP_LOGI(TAG, "=== SD CARD INIT (SPI3_HOST, independent bus) ===");
    ESP_LOGI(TAG, "CLK=%d, MOSI=%d, MISO=%d, CS=%d",
             BSP_SD_CLK, BSP_SD_MOSI, BSP_SD_MISO, BSP_SD_CS);

    /* 初始化独立SPI3总线给SD卡（LCD在SPI2_HOST上，不共享） */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_SD_MOSI,
        .miso_io_num = BSP_SD_MISO,
        .sclk_io_num = BSP_SD_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4096,
    };
    esp_err_t bus_ret = spi_bus_initialize(BSP_SD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_LOGI(TAG, "spi_bus_initialize(SPI3_HOST): %d (%s)", bus_ret,
             bus_ret == ESP_OK ? "OK" : (bus_ret == ESP_ERR_INVALID_STATE ? "ALREADY" : "FAIL"));

    /* SD卡SPI模式 */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BSP_SD_HOST;
    host.max_freq_khz = 400;  /* SD init requires ≤400kHz */

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_CS;
    slot_config.host_id = BSP_SD_HOST;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_card_t *card = NULL;
    ESP_LOGI(TAG, "Calling esp_vfs_fat_sdspi_mount...");
    esp_err_t ret = esp_vfs_fat_sdspi_mount(MP, &host, &slot_config,
                                             &mount_config, &card);
    ESP_LOGI(TAG, "Mount result: %d (%s)", ret, esp_err_to_name(ret));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: 0x%x", ret);
        return ret;
    }

    sdmmc_card_print_info(stdout, card);
    mounted = true;
    ESP_LOGI(TAG, "=== SD mounted at %s ===", MP);
    return ESP_OK;
}

bool sd_card_is_mounted(void) { return mounted; }
const char *sd_card_mount_point(void) { return MP; }
