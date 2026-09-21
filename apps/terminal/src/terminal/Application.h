// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "platform/vulkan/VulkanContext.h"
#include "platform/vulkan/VulkanSwapchain.h"
#include "platform/window/Window.h"
#include "ui/ImGuiLayer.h"
#include "ui/Workspace.h"

namespace terminal {

class Application {
public:
    Application();
    ~Application() = default;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

private:
    void rebuildSwapchainIfNeeded();

    GlfwContext glfw_;
    float scale_ = 1.0f;
    Window window_;
    VulkanContext vulkan_;
    VulkanSwapchain swapchain_;
    ImGuiLayer imgui_;
    Workspace workspace_;
};

}  // namespace terminal
