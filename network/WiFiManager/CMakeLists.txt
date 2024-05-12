cmake_minimum_required(VERSION 3.14.0)
project(WiFiManager VERSION 0.1.0 LANGUAGES C CXX)
get_verstring(VERSTRING)

#add_compile_options(-Os -m68040  -Wextra -Werror -fomit-frame-pointer)

set (CMAKE_CXX_STANDARD 17)

add_link_options(-m68040 -m68881 -s -noixemul -ffreestanding -nostdlib -nostartfiles -Wl,-e,__start) # -Wl,--gc-sections)
add_compile_options(-Os -m68040 -fomit-frame-pointer -Wall -fno-rtti -nostdlib -ffreestanding -fno-exceptions) # -ffunction-sections)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}" NO_EXCEPTIONS)
set_source_files_properties(src/start.cc PROPERTIES COMPILE_FLAGS -fno-function-sections)

add_executable(WiFiManager
    src/start.cc
    src/main.cc
    src/config.cc
    src/memory.cc
    src/string.cc
)

set_target_properties(WiFiManager PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(WiFiManager amiga m common sana2)
target_include_directories(WiFiManager PRIVATE include)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/WiFiManager DESTINATION ./Emu68-WiFi/Tools)
