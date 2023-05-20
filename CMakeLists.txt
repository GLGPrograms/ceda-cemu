cmake_minimum_required(VERSION 3.24)

project(ceda VERSION 2023.06)

add_executable(ceda
    src/bus.c
    src/ceda.c
    src/cpu.c
    src/main.c
    src/ram/dynamic.c
    src/rom/bios.c
)

set(CMAKE_C_STANDARD 17)

include_directories(vendor/Z80/API)
set(Z80_SHARED_LIBS NO CACHE BOOL "")
set(Z80_WITH_HTML_DOCUMENTATION YES)
add_subdirectory(vendor/Z80)

target_link_libraries(ceda
    Z80
)

