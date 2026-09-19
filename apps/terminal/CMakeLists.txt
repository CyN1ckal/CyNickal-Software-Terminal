find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)

set(TERMINAL_IMGUI_DIR ${CMAKE_SOURCE_DIR}/deps/imgui)
set(TERMINAL_CATCH_DIR ${CMAKE_SOURCE_DIR}/deps/catch)

add_executable(terminal
    src/main.cpp
    src/terminal/Application.cpp
    src/platform/vulkan/VulkanContext.cpp
    src/platform/vulkan/VulkanSwapchain.cpp
    src/platform/vulkan/VulkanUtils.cpp
    src/platform/window/Window.cpp
    src/ui/ImGuiLayer.cpp
    src/ui/Theme.cpp
    src/ui/Workspace.cpp
    ${TERMINAL_IMGUI_DIR}/imgui.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_draw.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_tables.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_widgets.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_demo.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_impl_glfw.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_impl_vulkan.cpp
)

if(MYAPP_ENABLE_CLANG_TIDY)
    set_target_properties(terminal PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
    )
    set_source_files_properties(
            ${TERMINAL_IMGUI_DIR}/imgui.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_draw.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_tables.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_widgets.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_demo.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_impl_glfw.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_impl_vulkan.cpp
            PROPERTIES SKIP_LINTING ON
    )
endif()

target_compile_options(terminal PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

target_include_directories(terminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TERMINAL_IMGUI_DIR}
)

target_compile_definitions(terminal PRIVATE
    VK_PROTOTYPES
)

target_link_libraries(terminal PRIVATE
    market-data
    glfw
    Vulkan::Vulkan
)

if(UNIX AND NOT APPLE)
    target_link_libraries(terminal PRIVATE ${CMAKE_DL_LIBS})
endif()

if(CYN_TESTING)
    add_executable(terminal_tests
        tests/test_main.cpp
        tests/data/bar_loading_tests.h
        ${TERMINAL_CATCH_DIR}/catch_amalgamated.cpp
    )
    target_include_directories(terminal_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${TERMINAL_CATCH_DIR}
    )
    target_link_libraries(terminal_tests PRIVATE
        market-data
    )
    target_compile_options(terminal_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    )
    add_test(NAME terminal_tests COMMAND terminal_tests)

    add_custom_target(run_tests
        COMMAND $<TARGET_FILE:terminal_tests>
        DEPENDS terminal_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running terminal_tests"
        USES_TERMINAL
        VERBATIM
    )
    add_dependencies(terminal run_tests)

    if(MYAPP_ENABLE_CLANG_TIDY)
        set_target_properties(terminal_tests PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--quiet;-extra-arg=-Wno-unknown-warning-option"
        )
        set_source_files_properties(
            ${TERMINAL_CATCH_DIR}/catch_amalgamated.cpp
            PROPERTIES SKIP_LINTING ON
        )
    endif()
endif()
