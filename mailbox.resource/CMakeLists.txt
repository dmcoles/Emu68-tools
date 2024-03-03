cmake_minimum_required(VERSION 3.14.0)
project(mailbox VERSION 1.0.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -nostdlib -nostartfiles -s -Wl,-e_rom_start -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
add_compile_options(-Os -m68040 -msmall-code -mpcrel -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

set_source_files_properties(src/main.c PROPERTIES COMPILE_FLAGS -mno-pcrel)

add_library(mailbox INTERFACE)

add_executable(mailbox.resource
    src/main.c
    src/init.c
)

target_include_directories(mailbox.resource PRIVATE include)
target_include_directories(mailbox INTERFACE include)

target_link_libraries(mailbox.resource common mailbox devicetree)

add_custom_command(
    TARGET mailbox.resource POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -v -O binary "${CMAKE_CURRENT_BINARY_DIR}/mailbox.resource" "${CMAKE_CURRENT_BINARY_DIR}/mailbox.bin"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(mailbox.resource PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds)
