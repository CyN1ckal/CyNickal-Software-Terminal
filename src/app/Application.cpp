#include "app/Application.h"

#include "imgui_impl_glfw.h"

#include <chrono>
#include <thread>

namespace myapp {
namespace {

constexpr int kBaseWindowWidth = 1280;
constexpr int kBaseWindowHeight = 800;
constexpr auto kWindowTitle = "MyApp";

}  // namespace

Application::Application()
    : scale_(ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()))
    , window_(static_cast<int>(kBaseWindowWidth * scale_),
              static_cast<int>(kBaseWindowHeight * scale_),
              kWindowTitle)
    , vulkan_(GlfwContext::requiredVulkanInstanceExtensions())
    , swapchain_(vulkan_, window_.createSurface(vulkan_.instance(), vulkan_.allocator()),
                 window_.framebufferSize())
    , imgui_(window_.handle(), vulkan_, swapchain_, scale_)
{
}

int Application::run()
{
    while (!window_.shouldClose())
    {
        Window::pollEvents();
        rebuildSwapchainIfNeeded();

        if (window_.isIconified())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        ImGuiLayer::newFrame();
        ui_.draw();
        ImGuiLayer::render(swapchain_, Workspace::clearColor());
    }

    return 0;
}

void Application::rebuildSwapchainIfNeeded()
{
    const auto [width, height] = window_.framebufferSize();
    if (width > 0 && height > 0 &&
        (swapchain_.needsRebuild() || swapchain_.width() != width ||
         swapchain_.height() != height))
    {
        swapchain_.resize(width, height);
    }
}

}  // namespace myapp
