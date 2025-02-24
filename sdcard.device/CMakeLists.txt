cmake_minimum_required(VERSION 3.14.0)
project(brcm-sdhc VERSION 1.6.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -s -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68040 -msmall-code -mpcrel -mregparm=4 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

set_source_files_properties(src/main.c PROPERTIES COMPILE_FLAGS -mno-pcrel)

add_executable(brcm-sdhc.device
    src/main.c
    src/expunge.c
    src/extfunc.c
    src/open.c
    src/close.c
    src/init.c
    src/beginio.c
    src/abortio.c
    src/mbox.c
    src/emmc.c
    src/sdhost.c
    src/unit_task.c
    src/findtoken.c
)

add_custom_command(
    TARGET brcm-sdhc.device POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/brcm-sdhc.device" "${CMAKE_CURRENT_BINARY_DIR}/brcm-sdhc.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(brcm-sdhc.device PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
target_link_libraries(brcm-sdhc.device devicetree)
