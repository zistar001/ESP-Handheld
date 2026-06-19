# retro-go-firmware main component

# Emulator main files from retro-core (copied to project deps/)
set(EMU_SRCS
    "../deps/retro-core-mains/main_nes.c"
    "../deps/retro-core-mains/main_gbc.c"
    "../deps/retro-core-mains/main_gw.c"
    "../deps/retro-core-mains/main_lynx.cpp"
    "../deps/retro-core-mains/main_pce.c"
    "../deps/retro-core-mains/main_sms.c"
    "../deps/retro-core-mains/main_snes.c"
)

# Launcher UI source files (from D:/retro-go/launcher/main/)
set(LAUNCHER_SRCS
    "../deps/retro-core-mains/applications.c"
    "../deps/retro-core-mains/bookmarks.c"
    "../deps/retro-core-mains/browser.c"
    "../deps/retro-core-mains/gui.c"
    "../deps/retro-core-mains/images.c"
    "../deps/retro-core-mains/updater.c"
)

idf_component_register(
    SRCS
        "main.c"
        "launcher_integration.c"
        ${EMU_SRCS}
        ${LAUNCHER_SRCS}
    INCLUDE_DIRS
        "."
        "../deps/retro-core-mains"
    REQUIRES
        retro-go
        nofrendo gnuboy pce-go gw-emulator handy smsplus snes9x
)

# Target definition + retro-go flags
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    RG_TARGET_ESP_BSP_HANDHELD=1
    RETRO_GO=1
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -Wno-error=format
    -Wno-error=char-subscripts
    -Wno-unused-function
)
