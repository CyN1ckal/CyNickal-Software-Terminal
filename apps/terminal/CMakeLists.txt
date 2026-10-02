# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(CURL REQUIRED)

set(TERMINAL_IMGUI_DIR ${CMAKE_SOURCE_DIR}/deps/imgui)
set(TERMINAL_IMPLOT_DIR ${CMAKE_SOURCE_DIR}/deps/implot)
set(TERMINAL_CATCH_DIR ${CMAKE_SOURCE_DIR}/deps/catch)

add_library(terminal_compile INTERFACE)
target_include_directories(terminal_compile INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/apps/common
    ${TERMINAL_IMGUI_DIR}
)
target_include_directories(terminal_compile SYSTEM INTERFACE
    ${TERMINAL_IMPLOT_DIR}
)
target_compile_definitions(terminal_compile INTERFACE VK_PROTOTYPES)
target_compile_options(terminal_compile INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)
target_link_libraries(terminal_compile INTERFACE
    market-data
    nlohmann_json
    glfw
    Vulkan::Vulkan
    CURL::libcurl
)

# Domain code shared by the app and the tests. It does not use terminal_compile:
# that interface pulls in Vulkan, GLFW, ImGui, and CURL, which this code does not.
# An OBJECT library keeps every translation unit, including the study and strategy
# registrars that nothing calls by name.
add_library(terminal_logic OBJECT
    src/data/StatementSheet.cpp
    src/data/StatementLineLabel.cpp
    src/data/PortfolioFetch.cpp
    src/data/LedgerMarks.cpp
    src/data/LedgerAnalysis.cpp
    src/risk/HistoricalRisk.cpp
    src/trading/Ledger.cpp
    src/trading/EquityCurve.cpp
    src/trading/TradeStats.cpp
    src/ui/TradingFormat.cpp
    src/ui/AppearanceSettings.cpp
    src/ui/CommandMatch.cpp
    src/backtest/Strategy.cpp
    src/backtest/Engine.cpp
    src/backtest/BacktestJob.cpp
    src/backtest/BacktestWorker.cpp
    src/backtest/strategies/MaCross.cpp
    src/backtest/strategies/BollingerRevert.cpp
    src/backtest/strategies/Momentum.cpp
    src/options/OptionPayoff.cpp
    src/options/OptionStrategy.cpp
    src/chart/CChartAxis.cpp
    src/chart/CChartCommand.cpp
    src/chart/CChartLoad.cpp
    src/chart/CSymbolLink.cpp
    src/chart/CChartTransform.cpp
    src/chart/CChartTrades.cpp
    src/chart/CChartbookDocument.cpp
    src/chart/CChartbookFile.cpp
    src/chart/CStudyCompute.cpp
    src/chart/studies/CMovingAverage.cpp
    src/chart/studies/CVolume.cpp
    src/chart/studies/CBollinger.cpp
    src/chart/studies/CNBarPercentChange.cpp
    src/chart/studies/StudyRegistry.cpp
)
target_include_directories(terminal_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(terminal_logic PRIVATE ${CMAKE_SOURCE_DIR}/apps/common)
target_link_libraries(terminal_logic PUBLIC market-data nlohmann_json)
target_compile_options(terminal_logic PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)
target_precompile_headers(terminal_logic PRIVATE ${TERMINAL_PCH_STL})

add_library(terminal_core OBJECT
    src/platform/vulkan/VulkanUtils.cpp
    src/platform/window/Window.cpp
    src/data/IngestWorker.cpp
    ${CMAKE_SOURCE_DIR}/apps/common/CurlClient.cpp
)
target_link_libraries(terminal_core PRIVATE terminal_compile)
target_precompile_headers(terminal_core PRIVATE ${TERMINAL_PCH_STL})

# Vendored ImGui and ImPlot .cpp files stay on the executable. They define
# IMGUI_DEFINE_MATH_OPERATORS before including imgui.h, which a precompiled
# imgui.h would break.
add_library(terminal_gui OBJECT
    src/main.cpp
    src/terminal/Application.cpp
    src/platform/vulkan/VulkanContext.cpp
    src/platform/vulkan/VulkanSwapchain.cpp
    src/ui/ImGuiLayer.cpp
    src/ui/Theme.cpp
    src/ui/Appearance.cpp
    src/ui/TitleBar.cpp
    src/ui/Workspace.cpp
    src/ui/CommandPalette.cpp
    src/ui/Preferences.cpp
    src/ui/ChartbookHost.cpp
    src/ui/StatusRail.cpp
    src/ui/InventoryPanel.cpp
    src/ui/ReceivedStamp.cpp
    src/ui/FinancialsPanel.cpp
    src/ui/OptionsChainPanel.cpp
    src/ui/OptionChainSource.cpp
    src/ui/PayoffWizard.cpp
    src/ui/PayoffPanel.cpp
    src/ui/PortfolioPanel.cpp
    src/ui/TradingWidgets.cpp
    src/ui/LedgerPanel.cpp
    src/ui/StatsPanel.cpp
    src/ui/BacktestPanel.cpp
    src/chart/CChartBook.cpp
    src/chart/CChartPane.cpp
    src/chart/CChartPlot.cpp
    src/chart/CStudyPlot.cpp
    src/chart/CStudySettings.cpp
)
target_link_libraries(terminal_gui PRIVATE terminal_compile)
target_precompile_headers(terminal_gui PRIVATE
    ${TERMINAL_PCH_STL}
    <vulkan/vulkan.h>
    <GLFW/glfw3.h>
    <imgui.h>
    <implot.h>
)

if(TERMINAL_ENABLE_CLANG_TIDY)
    set_target_properties(terminal_logic terminal_core terminal_gui PROPERTIES
        CXX_CLANG_TIDY "${TERMINAL_CLANG_TIDY}"
    )
endif()

add_executable(terminal
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

# deps/implot is third-party. Its diagnostics are not ours.
set_source_files_properties(
    ${TERMINAL_IMPLOT_DIR}/implot.cpp
    ${TERMINAL_IMPLOT_DIR}/implot_items.cpp
    PROPERTIES
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-w>$<$<CXX_COMPILER_ID:MSVC>:/w>"
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
)
target_include_directories(terminal SYSTEM PRIVATE
    ${TERMINAL_IMPLOT_DIR}
)

target_compile_definitions(terminal PRIVATE
    VK_PROTOTYPES
)

target_link_libraries(terminal PRIVATE
    terminal_logic
    terminal_core
    terminal_gui
    market-data
    nlohmann_json
    glfw
    Vulkan::Vulkan
    CURL::libcurl
)

if(WIN32)
    target_link_libraries(terminal PRIVATE dwmapi)
endif()

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(terminal PRIVATE ${CMAKE_DL_LIBS} X11::X11)
endif()

if(CYN_TESTING)
    add_executable(terminal_tests
        tests/test_main.cpp
        tests/chart/chart_command_tests.h
        tests/chart/chart_load_tests.h
        tests/chart/chart_study_tests.h
        tests/chart/chart_view_tests.h
        tests/chart/chart_transform_tests.h
        tests/chart/chartbook_file_tests.h
        ${TERMINAL_CATCH_DIR}/catch_amalgamated.cpp
    )
    target_include_directories(terminal_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/apps/common
        ${CMAKE_SOURCE_DIR}/libs/market-data/tests
        ${TERMINAL_CATCH_DIR}
    )
    target_link_libraries(terminal_tests PRIVATE
        terminal_logic
        market-data
        nlohmann_json
    )
    target_compile_options(terminal_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
    )
    target_precompile_headers(terminal_tests PRIVATE ${TERMINAL_PCH_STL})
    set_source_files_properties(
        ${TERMINAL_CATCH_DIR}/catch_amalgamated.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
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
