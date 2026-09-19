cmake_minimum_required(VERSION 4.0)
project(MyApp LANGUAGES CXX)

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

option(MYAPP_ENABLE_CLANG_TIDY "Run clang-tidy while compiling first-party targets" ON)
option(CYN_TESTING "Build Catch2 tests" ON)

if(MYAPP_ENABLE_CLANG_TIDY)
    message("clang-tidy running with build!")
    find_program(CLANG_TIDY_EXE
            NAMES clang-tidy
            REQUIRED
    )
endif()

if(CYN_TESTING)
    enable_testing()
endif()

add_subdirectory(apps/terminal)
