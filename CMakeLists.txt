cmake_minimum_required(VERSION 3.24)
project(gbemu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GB_WARNING_FLAGS -Wall -Wextra -Werror)

# sanitizers unavailable on mingw; ci covers them on linux
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(gbcore STATIC core/cartridge.cpp core/cpu.cpp core/gameboy.cpp core/mapper_rom.cpp)
target_include_directories(gbcore PUBLIC core)
target_compile_options(gbcore PRIVATE ${GB_WARNING_FLAGS})

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    add_executable(gbemu-sdl frontend/sdl/main.cpp)
    target_compile_options(gbemu-sdl PRIVATE ${GB_WARNING_FLAGS})
    target_link_libraries(gbemu-sdl PRIVATE gbcore SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(gbemu-sdl PRIVATE SDL2::SDL2main)
    endif()
else()
    message(WARNING "sdl2 not found; skipping gbemu-sdl target")
endif()

include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(catch2)

enable_testing()
add_executable(gbcore_tests tests/unit/cartridge_test.cpp tests/unit/cpu_alu_test.cpp
                            tests/unit/cpu_blargg_smoke_test.cpp tests/unit/cpu_decode_test.cpp
                            tests/unit/cpu_test.cpp tests/unit/mapper_rom_test.cpp
                            tests/unit/smoke_test.cpp)
target_compile_options(gbcore_tests PRIVATE ${GB_WARNING_FLAGS})
target_link_libraries(gbcore_tests PRIVATE gbcore Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(gbcore_tests)

option(GB_BUILD_FUZZERS "build libfuzzer targets (clang only)" OFF)
if(GB_BUILD_FUZZERS)
    add_executable(fuzz_header tests/fuzz/fuzz_header.cpp core/cartridge.cpp core/mapper_rom.cpp)
    target_include_directories(fuzz_header PRIVATE core)
    target_compile_options(fuzz_header PRIVATE -fsanitize=fuzzer,address,undefined ${GB_WARNING_FLAGS})
    target_link_options(fuzz_header PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
