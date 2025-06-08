cmake_minimum_required(VERSION 3.22)

project(ceda VERSION 2023.06)

# Put here sources needed for the core functionalities of the emulator
set(CORE_SRCS
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
)

# Put here sources needed for tests only
set(TEST_SRCS
    src/tests/test_fdc.c
)

# External dependencies
add_library(inih STATIC vendor/inih/ini.c)
include_directories(vendor/inih)

include_directories(vendor/Z80/API)
set(Z80_SHARED_LIBS NO CACHE BOOL "")
add_subdirectory(vendor/Z80)

# Global properties and settings
set(CMAKE_C_STANDARD 17)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "--use-color" "--extra-arg=-Wno-unknown-warning-option")

set_source_files_properties(src/3rd/disassembler.c PROPERTIES COMPILE_FLAGS -Wno-discarded-qualifiers)

# Automatically add targets, with same properties
function(add_ceda_target target)

    add_executable(${target}
        ${CORE_SRCS}
    )

    target_compile_options(${target} PRIVATE -W -Wformat -Wall -Wundef -Wpointer-arith -Wcast-qual -Wwrite-strings -Wsign-compare -Wmissing-noreturn -Wextra -Wconversion)

    if(CMAKE_BUILD_TYPE MATCHES "Release")
        target_compile_options(${target} PRIVATE -Werror)
    elseif(CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_options(${target} PRIVATE -DDEBUG=1)
        # set(Z80_WITH_HTML_DOCUMENTATION YES)
    endif()

    target_link_libraries(${target}
        Z80
        SDL2
        SDL2_mixer
        inih
    )

    set_target_properties(${target} PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_COMMAND}")

endfunction()

# GUI target and tests target
add_ceda_target(ceda)
add_ceda_target(ceda-test)

# Options related to test target only
target_compile_options(ceda-test PRIVATE -DCEDA_TEST=1)

target_link_libraries(ceda-test
    criterion
)

target_sources(ceda-test
    PRIVATE
    ${TEST_SRCS}
)
