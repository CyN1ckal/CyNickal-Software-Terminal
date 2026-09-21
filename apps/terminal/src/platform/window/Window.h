#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <utility>
#include <vector>

namespace terminal {

class GlfwContext {
public:
    GlfwContext();
    ~GlfwContext();

    GlfwContext(const GlfwContext&) = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;
    GlfwContext(GlfwContext&&) = delete;
    GlfwContext& operator=(GlfwContext&&) = delete;

    [[nodiscard]] static std::vector<const char*> requiredVulkanInstanceExtensions();
};

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] GLFWwindow* handle() const noexcept { return window_; }
    [[nodiscard]] bool shouldClose() const noexcept;
    [[nodiscard]] bool isIconified() const noexcept;
    [[nodiscard]] std::pair<int, int> framebufferSize() const;

    static void pollEvents();
    [[nodiscard]] VkSurfaceKHR createSurface(VkInstance instance,
                                             const VkAllocationCallbacks* allocator) const;

private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace terminal
