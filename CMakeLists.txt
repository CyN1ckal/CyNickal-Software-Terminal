cmake_minimum_required(VERSION 3.16)
project(MyApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_vulkan.cpp
)

target_compile_options(MyApp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra>
)

target_include_directories(MyApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
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
