cmake_minimum_required(VERSION 3.14.0)
project(Emu68rom VERSION 1.0.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -s -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68040 -msmall-code -mpcrel -mregparm=4 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

set_source_files_properties(src/main.c PROPERTIES COMPILE_FLAGS -mno-pcrel)

add_executable(Emu68rom
    src/main.c
    src/init.c
)

add_custom_command(
    TARGET Emu68rom POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/Emu68rom" "${CMAKE_CURRENT_BINARY_DIR}/emu68rom.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(Emu68rom PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
