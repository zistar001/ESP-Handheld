file(GLOB nofrendo_src "*.c" "nes/*.c" "mappers/map*.c")
idf_component_register(SRCS ${nofrendo_src} INCLUDE_DIRS "." REQUIRES retro-go)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -Wno-array-bounds -Wno-error=format -Wno-format)
