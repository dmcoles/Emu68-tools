cmake_minimum_required(VERSION 3.14.0)
project(68040 VERSION 1.3.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -s -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68040 -msmall-code -mpcrel -mregparm=4 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

set_source_files_properties(src/main.c PROPERTIES COMPILE_FLAGS -mno-pcrel)

add_executable(68040.library
    src/main.c
    src/init.c
    src/cachepredma.c
    src/cachepostdma.c
)

add_custom_command(
    TARGET 68040.library POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/68040.library" "${CMAKE_CURRENT_BINARY_DIR}/68040.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(68040.library PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
target_link_libraries(68040.library devicetree)