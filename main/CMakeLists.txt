idf_component_register(
    SRCS "main.c"
          "bsp/bsp_board.c" "bsp/st7789_driver.c"
          "bsp/sd_card.c"
          "modules/sensor/aht20_driver.c"
          "modules/imu/imu_driver.c"
    INCLUDE_DIRS "."
                 "bsp"
                 "modules/sensor"
                 "modules/imu"
    REQUIRES driver esp_timer spi_flash fatfs sdmmc)
