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
    src/Application.cpp
    src/ImGuiLayer.cpp
    src/Theme.cpp
    src/Workspace.cpp
    src/VulkanContext.cpp
    src/VulkanSwapchain.cpp
    src/VulkanUtils.cpp
    src/Window.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_vulkan.cpp
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
