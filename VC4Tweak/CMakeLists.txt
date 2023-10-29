cmake_minimum_required(VERSION 3.14.0)
project(VC4Tweak VERSION 1.0.0)
get_verstring(VERSTRING)

add_link_options(-ffreestanding -m68030 -nostdlib -nostartfiles -Wl,-e,__start)
add_compile_options(-Os -m68030 -fomit-frame-pointer)
add_compile_definitions(PRIVATE VERSION_STRING="${VERSTRING}")

add_executable(VC4Tweak
    src/main.c
)

target_link_libraries(VC4Tweak mui amiga devicetree VideoCore)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/VC4Tweak DESTINATION ./VC4Tweak/)
# install(FILES EmuControl.info DESTINATION ./EmuControl/)
