idf_component_register(
    SRCS "main.c"
          # BSP
          "bsp/bsp_board.c" "bsp/st7789_driver.c" "bsp/sd_card.c"
          # App framework
          "app/app_manager.c" "app/launcher.c" "app/menu.c" "app/rom_browser.c"
          # NES emulator
          "modules/nes/nes_game.c" "modules/nes/nes_wrapper.c" "modules/nes/font8x16.c"
          # Audio
          "modules/audio/box_audio_codec.c" "modules/audio/es8311_driver.c" "modules/audio/es7210_driver.c"
          # Sensors
          "modules/sensor/aht20_driver.c"
          "modules/imu/imu_driver.c"
          # Power
          "modules/power/battery_monitor.c"
          # Settings
          "modules/settings/settings_manager.c"
          # Time sync
          "modules/time_sync/time_sync.c"
          # Weather
          "modules/weather/weather.c"
          # WiFi
          "modules/wifi_manager/wifi_manager.c" "modules/wifi_manager/wifi_bridge.cpp"
          # XiaoZhi wrapper
          "modules/xiaozhi/xiaozhi_wrapper.c"
          # UI
          "ui/display_driver.c"
          "ui/components/status_bar.c"
          "ui/screens/home_screen.c" "ui/screens/menu_screen.c"
          "ui/screens/settings_screen.c"
          "ui/screens/countdown_screen.c"
    INCLUDE_DIRS "."
                 "bsp"
                 "app"
                 "modules/nes"
                 "modules/audio"
                 "modules/sensor"
                 "modules/imu"
                 "modules/power"
                 "modules/settings"
                 "modules/time_sync"
                 "modules/weather"
                 "modules/wifi_manager"
                 "modules/xiaozhi"
                 "ui"
                 "ui/components"
                 "ui/screens"
    REQUIRES driver esp_timer spi_flash nvs_flash fatfs sdmmc app_update esp_partition esp_hid esp_adc
             esp_lcd nes_core espressif__esp_codec_dev lvgl
             esp_wifi esp_event esp_netif esp_http_client json
             esp_pm esp_http_server mbedtls zlib)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format-truncation)
