#pragma once

#include "ImGuiLayer.h"
#include "Workspace.h"
#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include "Window.h"

namespace myapp {

class Application {
public:
    Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    void rebuildSwapchainIfNeeded();

    GlfwContext glfw_;
    float scale_ = 1.0f;
    Window window_;
    VulkanContext vulkan_;
    VulkanSwapchain swapchain_;
    ImGuiLayer imgui_;
    Workspace ui_;
};

}  // namespace myapp
