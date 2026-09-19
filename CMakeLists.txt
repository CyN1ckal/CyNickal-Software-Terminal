cmake_minimum_required(VERSION 4.0)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# CLion/Ninja + clang-tidy: CMake 3.28+ scans C++20 TUs for modules and
# injects GCC -fmodules-ts flags that LLVM clang-tidy rejects as errors.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)

add_executable(MyApp
    src/main.cpp
    src/app/Application.cpp
    src/platform/vulkan/VulkanContext.cpp
    src/platform/vulkan/VulkanSwapchain.cpp
    src/platform/vulkan/VulkanUtils.cpp
    src/platform/window/Window.cpp
    src/ui/ImGuiLayer.cpp
    src/ui/Theme.cpp
    src/ui/Workspace.cpp
    deps/imgui/imgui.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_impl_glfw.cpp
    deps/imgui/imgui_impl_vulkan.cpp
        src/data/CBarData.cpp
        src/data/CBarData.h
        src/data/CBarLoader.cpp
        src/data/CBarLoader.h
)

option(MYAPP_ENABLE_CLANG_TIDY "Run clang-tidy while compiling MyApp" ON)

if(MYAPP_ENABLE_CLANG_TIDY)
    message("clang-tidy running with build!")
    find_program(CLANG_TIDY_EXE
            NAMES clang-tidy
            REQUIRED
    )
    set_target_properties(MyApp PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
    )
    set_source_files_properties(
            deps/imgui/imgui.cpp
            deps/imgui/imgui_draw.cpp
            deps/imgui/imgui_tables.cpp
            deps/imgui/imgui_widgets.cpp
            deps/imgui/imgui_demo.cpp
            deps/imgui/imgui_impl_glfw.cpp
            deps/imgui/imgui_impl_vulkan.cpp
            PROPERTIES SKIP_LINTING ON
    )
endif()

target_compile_options(MyApp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

target_include_directories(MyApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui
)

target_compile_definitions(MyApp PRIVATE
    VK_PROTOTYPES
)

target_link_libraries(MyApp PRIVATE
    glfw
    Vulkan::Vulkan
)

if(UNIX AND NOT APPLE)
    target_link_libraries(MyApp PRIVATE ${CMAKE_DL_LIBS})
endif()

option(CYN_TESTING "Build Catch2 tests" ON)

if(CYN_TESTING)
    enable_testing()
    add_executable(MyApp_tests
        tests/test_main.cpp
        tests/data/bar_loading_tests.h
        src/data/CBarData.cpp
        src/data/CBarLoader.cpp
        deps/catch/catch_amalgamated.cpp
    )
    target_include_directories(MyApp_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/catch
    )
    target_compile_options(MyApp_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    )
    add_test(NAME MyApp_tests COMMAND MyApp_tests)

    add_custom_target(run_tests
        COMMAND $<TARGET_FILE:MyApp_tests>
        DEPENDS MyApp_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running MyApp_tests"
        USES_TERMINAL
        VERBATIM
    )
    add_dependencies(MyApp run_tests)

    if(MYAPP_ENABLE_CLANG_TIDY)
        set_target_properties(MyApp_tests PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
        )
        set_source_files_properties(
            deps/catch/catch_amalgamated.cpp
            PROPERTIES SKIP_LINTING ON
        )
    endif()
endif()
