cmake_minimum_required(VERSION 3.14.0)
project(WiFiPi VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e,__start)
add_compile_options(-Os -m68040 -fomit-frame-pointer -mno-bitfield)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(wifipi.device
    src/start.c
    src/device.c
    src/init.c
    src/mbox.c
    src/sdio.c
    src/end.c
)
target_link_libraries(wifipi.device devicetree amiga)

add_executable(wifipi.rom
    src/start.c
    src/device.c
    src/init.c
    src/mbox.c
    src/sdio.c
    src/end.c
)
target_link_libraries(wifipi.rom devicetree)
target_link_options(wifipi.rom PRIVATE -Wl,-e_rom_start -Wl,-T${CMAKE_SOURCE_DIR}/extrom.lds)
add_custom_command(
    TARGET wifipi.rom POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/wifipi.rom" "${CMAKE_CURRENT_BINARY_DIR}/wifipi.rom.bin"
    COMMAND rm -f "/home/michal/tftpboot/wifipi.rom"
    COMMAND dd if="${CMAKE_CURRENT_BINARY_DIR}/wifipi.rom.bin" of="/home/michal/tftpboot/wifipi.rom" bs=512k skip=30
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(wifipi.rom PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/extrom.lds)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/wifipi.device DESTINATION ./Emu68-WiFi/)
