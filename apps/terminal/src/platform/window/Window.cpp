// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "platform/window/Window.h"

#include "platform/vulkan/VulkanUtils.h"

#include <cstdio>
#include <stdexcept>

namespace terminal {
namespace {

void glfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

}  // namespace

GlfwContext::GlfwContext()
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == GLFW_FALSE)
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

GlfwContext::~GlfwContext()
{
    glfwTerminate();
}

std::vector<const char*> GlfwContext::requiredVulkanInstanceExtensions()
{
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr)
    {
        throw std::runtime_error("GLFW: Vulkan not supported");
    }

    return {extensions, extensions + count};
}

Window::Window(int width, int height, const char* title)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (glfwVulkanSupported() == GLFW_FALSE)
    {
        throw std::runtime_error("GLFW: Vulkan not supported");
    }

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

Window::~Window()
{
    glfwDestroyWindow(window_);
}

bool Window::shouldClose() const noexcept
{
    return glfwWindowShouldClose(window_) != GLFW_FALSE;
}

bool Window::isIconified() const noexcept
{
    return glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0;
}

std::pair<int, int> Window::framebufferSize() const
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return {width, height};
}

void Window::pollEvents()
{
    glfwPollEvents();
}

VkSurfaceKHR Window::createSurface(VkInstance instance, const VkAllocationCallbacks* allocator) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    checkVkResult(glfwCreateWindowSurface(instance, window_, allocator, &surface));
    return surface;
}

}  // namespace terminal
