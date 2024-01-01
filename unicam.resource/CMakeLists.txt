cmake_minimum_required(VERSION 3.14.0)
project(unicam VERSION 0.1.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68040 -msmall-code -mpcrel -mregparm=4 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

set_source_files_properties(src/main.c PROPERTIES COMPILE_FLAGS -mno-pcrel)

add_library(unicam INTERFACE)
target_include_directories(unicam INTERFACE include)

add_executable(unicam.resource
    src/main.c
    src/init.c
    src/mbox.c
    src/smoothing.c
    src/start.c
    src/stop.c
    src/unicam.c
    src/videocore.c
)

add_custom_command(
    TARGET unicam.resource POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/unicam.resource" "${CMAKE_CURRENT_BINARY_DIR}/unicam.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(unicam.resource PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
target_link_libraries(unicam.resource unicam devicetree common)
