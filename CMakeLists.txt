cmake_minimum_required(VERSION 3.22)

project(ceda VERSION 2023.06)

add_executable(ceda
    src/bus.c
    src/ceda.c
    src/ceda_string.c
    src/cli.c
    src/conf.c
    src/cpu.c
    src/crtc.c
    src/fdc.c
    src/floppy.c
    src/gui.c
    src/hexdump.c
    src/int.c
    src/keyboard.c
    src/main.c
    src/serial.c
    src/sio2.c
    src/speaker.c
    src/time.c
    src/timer.c
    src/tokenizer.c
    src/upd8255.c
    src/video.c
    src/ram/auxram.c
    src/ram/dynamic.c
    src/bios.c
    src/3rd/disassembler.c
    src/tests/test_fdc.c
)

target_compile_options(ceda PRIVATE -W -Wformat -Wall -Wundef -Wpointer-arith -Wcast-qual -Wwrite-strings -Wsign-compare -Wmissing-noreturn -Wextra -Wconversion)

set_source_files_properties(src/3rd/disassembler.c PROPERTIES COMPILE_FLAGS -Wno-discarded-qualifiers)

if(CEDA_TEST)
    target_compile_options(ceda PRIVATE -DCEDA_TEST=1)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release")
    target_compile_options(ceda PRIVATE -Werror)
elseif(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_options(ceda PRIVATE -DDEBUG=1)
    # set(Z80_WITH_HTML_DOCUMENTATION YES)
endif()

set(CMAKE_C_STANDARD 17)

add_library(inih STATIC vendor/inih/ini.c)
include_directories(vendor/inih)

include_directories(vendor/Z80/API)
set(Z80_SHARED_LIBS NO CACHE BOOL "")
add_subdirectory(vendor/Z80)

target_link_libraries(ceda
    Z80
    SDL2
    SDL2_mixer
    criterion
    inih
)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "--use-color" "--extra-arg=-Wno-unknown-warning-option")
set_target_properties(ceda PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_COMMAND}")

