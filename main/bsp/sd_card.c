#include "sd_card.h"
#include "bsp_board.h"
#include "st7789_driver.h"
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
    ESP_LOGI(TAG, "=== SD CARD INIT (SPI mode) ===");
    ESP_LOGI(TAG, "SCK=%d, MOSI=%d, MISO=%d, CS=%d",
             BSP_LCD_CLK, BSP_LCD_MOSI, GPIO_NUM_8, BSP_SD_CS);

    /* 先初始化SPI总线（如果LCD已初始化，会返回INVALID_STATE，忽略） */
    spi_host_device_t spi_host = st7789_get_spi_host();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_LCD_MOSI,
        .miso_io_num = GPIO_NUM_8,
        .sclk_io_num = BSP_LCD_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4096,
    };
    esp_err_t bus_ret = spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_LOGI(TAG, "spi_bus_initialize: %d (%s)", bus_ret,
             bus_ret == ESP_OK ? "OK" : (bus_ret == ESP_ERR_INVALID_STATE ? "ALREADY" : "FAIL"));

    /* SD卡SPI模式 */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = spi_host;
    host.max_freq_khz = 400;  /* SD init requires ≤400kHz */

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_CS;
    slot_config.host_id = st7789_get_spi_host();

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
