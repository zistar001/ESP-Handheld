idf_component_register(
    SRCS "main.c"
          "bsp/bsp_board.c" "bsp/st7789_driver.c"
    INCLUDE_DIRS "."
                 "bsp"
    REQUIRES driver esp_timer spi_flash
)
