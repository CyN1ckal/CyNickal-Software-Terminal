# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(CURL REQUIRED)

set(TERMINAL_IMGUI_DIR ${CMAKE_SOURCE_DIR}/deps/imgui)
set(TERMINAL_IMPLOT_DIR ${CMAKE_SOURCE_DIR}/deps/implot)
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
    src/ui/InventoryPanel.cpp
    src/data/IngestWorker.cpp
    src/chart/CChartLoad.cpp
    src/chart/CChartTransform.cpp
    src/chart/CStudyCompute.cpp
    src/chart/CStudySettings.cpp
    src/chart/CChartPane.cpp
    src/chart/CChartBook.cpp
    src/chart/CChartPlot.cpp
    ${CMAKE_SOURCE_DIR}/apps/common/CurlClient.cpp
    ${TERMINAL_IMGUI_DIR}/imgui.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_draw.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_tables.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_widgets.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_demo.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_impl_glfw.cpp
    ${TERMINAL_IMGUI_DIR}/imgui_impl_vulkan.cpp
    ${TERMINAL_IMPLOT_DIR}/implot.cpp
    ${TERMINAL_IMPLOT_DIR}/implot_items.cpp
)

if(TERMINAL_ENABLE_CLANG_TIDY)
    set_target_properties(terminal PROPERTIES
            CXX_CLANG_TIDY "${TERMINAL_CLANG_TIDY}"
    )
    set_source_files_properties(
            ${TERMINAL_IMGUI_DIR}/imgui.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_draw.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_tables.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_widgets.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_demo.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_impl_glfw.cpp
            ${TERMINAL_IMGUI_DIR}/imgui_impl_vulkan.cpp
            ${TERMINAL_IMPLOT_DIR}/implot.cpp
            ${TERMINAL_IMPLOT_DIR}/implot_items.cpp
            PROPERTIES SKIP_LINTING ON
    )
endif()

target_compile_options(terminal PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

target_include_directories(terminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/apps/common
    ${TERMINAL_IMGUI_DIR}
    ${TERMINAL_IMPLOT_DIR}
)

target_compile_definitions(terminal PRIVATE
    VK_PROTOTYPES
)

target_link_libraries(terminal PRIVATE
    market-data
    glfw
    Vulkan::Vulkan
    CURL::libcurl
)

if(UNIX AND NOT APPLE)
    target_link_libraries(terminal PRIVATE ${CMAKE_DL_LIBS})
endif()

if(CYN_TESTING)
    add_executable(terminal_tests
        tests/test_main.cpp
        src/chart/CChartLoad.cpp
        src/chart/CChartTransform.cpp
        src/chart/CStudyCompute.cpp
        tests/chart/chart_load_tests.h
        tests/chart/chart_study_tests.h
        tests/chart/chart_view_tests.h
        tests/chart/chart_transform_tests.h
        ${TERMINAL_CATCH_DIR}/catch_amalgamated.cpp
    )
    target_include_directories(terminal_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/libs/market-data/tests
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

    if(TERMINAL_ENABLE_CLANG_TIDY)
        set_target_properties(terminal_tests PROPERTIES
            CXX_CLANG_TIDY "${TERMINAL_CLANG_TIDY}"
        )
        set_source_files_properties(
            ${TERMINAL_CATCH_DIR}/catch_amalgamated.cpp
            PROPERTIES SKIP_LINTING ON
        )
    endif()
endif()
