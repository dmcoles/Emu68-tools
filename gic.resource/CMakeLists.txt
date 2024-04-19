cmake_minimum_required(VERSION 3.14.0)
project(gic VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68040 -msmall-code -mpcrel -mregparm=4 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

set_source_files_properties(src/main.c PROPERTIES COMPILE_FLAGS -mno-pcrel)

add_library(gic INTERFACE)
target_include_directories(gic INTERFACE include)

add_executable(gic.resource
    src/main.c
    src/init.c
    src/addintserver.c
    src/remintserver.c
)

add_custom_command(
    TARGET gic.resource POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/gic.resource" "${CMAKE_CURRENT_BINARY_DIR}/gic.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(gic.resource PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
target_link_libraries(gic.resource gic devicetree common)
