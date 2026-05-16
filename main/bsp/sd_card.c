#include "sd_card.h"
#include "bsp_board.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

static const char *TAG="SD";
static const char *MP="/sdcard";
static bool mounted=false;

esp_err_t sd_card_init(void){
    sdmmc_host_t host=SDSPI_HOST_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mc={.format_if_mount_failed=false,.max_files=5,.allocation_unit_size=16*1024};
    sdspi_device_config_t sc=SDSPI_DEVICE_CONFIG_DEFAULT();
    sc.gpio_cs=BSP_SD_CS; sc.host_id=BSP_LCD_HOST;
    sdmmc_card_t *card;
    esp_err_t r=esp_vfs_fat_sdspi_mount(MP,&host,&sc,&mc,&card);
    if(r!=ESP_OK){ ESP_LOGE(TAG,"SD mount fail %d",r); return r; }
    sdmmc_card_print_info(stdout,card);
    mounted=true;
    ESP_LOGI(TAG,"SD mounted at %s",MP);
    return ESP_OK;
}
bool sd_card_is_mounted(void){ return mounted; }
const char *sd_card_mount_point(void){ return MP; }
