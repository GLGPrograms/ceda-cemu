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
    src/monitor.c
    src/charmon.c
    src/serial.c
    src/sio2.c
    src/speaker.c
    src/time.c
    src/timer.c
    src/tokenizer.c
    src/ubus.c
    src/upd8255.c
    src/video.c
    src/ram/auxram.c
    src/ram/dynamic.c
    src/bios.c
    src/3rd/disassembler.c
)

# Put here sources needed for tests only
set(TEST_SRCS
    src/main_test.c

    src/ceda_string.c
    src/conf.c
    src/fdc.c
    src/hexdump.c
    src/monitor.c
    src/tokenizer.c

    src/tests/test_ceda_string.c
    src/tests/test_conf.c
    src/tests/test_fdc.c
    src/tests/test_tokenizer.c
    src/tests/test_monitor.c
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

add_compile_definitions(CEDA_PREFIX="${CMAKE_INSTALL_PREFIX}")

# Automatically add targets, with same properties
function(add_ceda_target target)

    target_compile_options(${target} PRIVATE -W -Wformat -Wall -Wundef -Wpointer-arith -Wcast-qual -Wwrite-strings -Wsign-compare -Wmissing-noreturn -Wextra -Wconversion -coverage)

    if(CMAKE_BUILD_TYPE MATCHES "Release")
        target_compile_options(${target} PRIVATE -Werror)
    elseif(CMAKE_BUILD_TYPE MATCHES "Debug")
        target_compile_options(${target} PRIVATE -DDEBUG=1)
    endif()

    target_link_libraries(${target}
        Z80
        SDL3
        SDL3_mixer
        inih
    )
    target_link_options(${target} PRIVATE -coverage)

    set_target_properties(${target} PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_COMMAND}")

endfunction()

# Options related to core target only
# -----------------------------------
add_executable(ceda ${CORE_SRCS})
add_ceda_target(ceda)
target_sources(ceda
    PRIVATE
    ${CORE_SRC}
)

# Options related to test target only
# -----------------------------------
include(CTest)
set(CMAKE_CTEST_ARGUMENTS "--verbose")
add_executable(ceda-test ${TEST_SRCS})
add_ceda_target(ceda-test)
add_test(NAME ceda-test
    COMMAND ceda-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ceda-test
    criterion
)

target_sources(ceda-test
    PRIVATE
    ${TEST_SRCS}
)

# External binary blobs / assets (eg. ROM and operating system disks)
# -------------------------------------------------------------------
include(ExternalProject)
ExternalProject_Add(asset_rom
    URL https://github.com/GLGPrograms/ceda-home/raw/refs/heads/main/assets/v1.01_rom.bin
    URL_HASH SHA256=e6c83290a92268a01a97469b568e770fc154ee435503c0c3b313bed61a5f820e
    DOWNLOAD_NO_EXTRACT True
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/asset_rom-prefix/src/v1.01_rom.bin
        ${CMAKE_BINARY_DIR}/asset_rom-prefix/v1.01_rom.bin
)
ExternalProject_Add(asset_char
    URL https://github.com/GLGPrograms/ceda-home/raw/refs/heads/main/assets/cgv7.2_rom.bin
    URL_HASH SHA256=4d52b5f43ab5ade0f37e6d2aca0d8cdfae6fcdf4598640a727dd6cf3e1c450b4
    DOWNLOAD_NO_EXTRACT True
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/asset_char-prefix/src/cgv7.2_rom.bin
        ${CMAKE_BINARY_DIR}/asset_char-prefix/cgv7.2_rom.bin
)
ExternalProject_Add(asset_disk
    URL https://github.com/GLGPrograms/ceda-cpm/releases/download/cpm22-vers1.7/SANCO-CPM22_us.bin
    URL_HASH SHA256=db9e29115690a158c9a18088eb3ee348c28eadad3f533623af9e7733949adb9f
    DOWNLOAD_NO_EXTRACT True
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/asset_disk-prefix/src/SANCO-CPM22_us.bin
        ${CMAKE_BINARY_DIR}/asset_disk-prefix/SANCO-CPM22_us.bin
)

# Installation
# ------------
install(TARGETS ceda
    RUNTIME
    DESTINATION bin
    COMPONENT application
)
install(FILES ${CMAKE_SOURCE_DIR}/conf/ceda-cemu.ini.template DESTINATION share/ceda)
install(FILES ${CMAKE_BINARY_DIR}/asset_rom-prefix/v1.01_rom.bin DESTINATION share/ceda)
install(FILES ${CMAKE_BINARY_DIR}/asset_char-prefix/cgv7.2_rom.bin DESTINATION share/ceda)
install(FILES ${CMAKE_BINARY_DIR}/asset_disk-prefix/SANCO-CPM22_us.bin DESTINATION share/ceda)

# Packaging
# ---------
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL application)
include(CPack)


