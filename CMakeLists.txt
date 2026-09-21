# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

cmake_minimum_required(VERSION 4.0)
project(terminal LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# CLion/Ninja + clang-tidy: CMake 3.28+ scans C++20 TUs for modules and
# injects GCC -fmodules-ts flags that LLVM clang-tidy rejects as errors.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Keep binaries at the build root (build/terminal) rather than
# build/apps/terminal/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(TERMINAL_ENABLE_CLANG_TIDY "Run clang-tidy while compiling first-party targets" ON)
option(CYN_TESTING "Build Catch2 tests" ON)

if(TERMINAL_ENABLE_CLANG_TIDY)
    message("clang-tidy running with build!")
    find_program(CLANG_TIDY_EXE
            NAMES clang-tidy
            REQUIRED
    )
    set(TERMINAL_CLANG_TIDY
        "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
    )
    if(MSVC)
        # CMake adds --driver-mode=cl for MSVC, but clang-tidy still treats
        # exceptions as disabled unless /EHsc is also an extra-arg.
        string(APPEND TERMINAL_CLANG_TIDY ";-extra-arg=/EHsc")
    endif()
endif()

if(CYN_TESTING)
    enable_testing()
endif()

add_subdirectory(libs/market-data)
add_subdirectory(apps/ingest)
add_subdirectory(apps/terminal)
