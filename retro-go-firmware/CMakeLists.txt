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

# Extra component dirs: our retro-go + emulator cores
# NOTE: emulator cores are expected at ../retro-go-core/ relative to this directory.
#       Clone https://github.com/ducalex/retro-go and set RETRO_GO_CORE_DIR to point to
#       its retro-core/components/ directory, or adjust paths below.
set(RETRO_GO_CORE_DIR "D:/retro-go/retro-core/components" CACHE PATH "Path to retro-go emulator cores")
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_SOURCE_DIR}/../components/retro-go"
    "${RETRO_GO_CORE_DIR}/nofrendo"
    "${RETRO_GO_CORE_DIR}/gnuboy"
    "${RETRO_GO_CORE_DIR}/pce-go"
    "${RETRO_GO_CORE_DIR}/gw-emulator"
    "${RETRO_GO_CORE_DIR}/handy"
    "${RETRO_GO_CORE_DIR}/smsplus"
    "${RETRO_GO_CORE_DIR}/snes9x"
)

# Components we need
set(COMPONENTS
    main
    retro-go
    nofrendo gnuboy pce-go gw-emulator handy smsplus snes9x
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(retro-go-firmware)
