cmake_minimum_required(VERSION 3.5)

# Target: ESP32-S3, ST7789 240x280, ES8311 audio, 7 keys, SD card, 8MB PSRAM
set(RG_BUILD_TARGET "RG_TARGET_ESP_BSP_HANDHELD")
set(RG_ENABLE_NETWORKING 0)
set(RG_ENABLE_NETPLAY 0)

# Define rg_setup_compile_options macro (same as D:\retro-go\base.cmake)
# Must be defined before project() for component CMakeLists to use it
macro(rg_setup_compile_options)
    component_compile_options(
        -D${RG_BUILD_TARGET}=1
        -DRETRO_GO=1
        -fjump-tables -ftree-switch-conversion
        ${ARGV}
    )
    if(IDF_TARGET STREQUAL "esp32")
        component_compile_options(-mfix-esp32-psram-cache-issue
            -mfix-esp32-psram-cache-strategy=memw)
    endif()
    if(RG_ENABLE_NETPLAY)
        component_compile_options(-DRG_ENABLE_NETWORKING -DRG_ENABLE_NETPLAY)
    elseif(RG_ENABLE_NETWORKING)
        component_compile_options(-DRG_ENABLE_NETWORKING)
    endif()
endmacro()

# Extra component dirs: our retro-go + emulator cores from D:\retro-go
set(EXTRA_COMPONENT_DIRS
    "D:/code/claudecode/ESP_BSP1/components/retro-go"
    "D:/retro-go/retro-core/components/nofrendo"
    "D:/retro-go/retro-core/components/gnuboy"
    "D:/retro-go/retro-core/components/pce-go"
    "D:/retro-go/retro-core/components/gw-emulator"
    "D:/retro-go/retro-core/components/handy"
    "D:/retro-go/retro-core/components/smsplus"
    "D:/retro-go/retro-core/components/snes9x"
)

# Components we need
set(COMPONENTS
    main
    retro-go
    nofrendo gnuboy pce-go gw-emulator handy smsplus snes9x
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(retro-go-firmware)
